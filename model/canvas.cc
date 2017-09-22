/*
  @copyright Steve Keen 2017
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "geometry.h"
#include "canvas.h"
#include "cairoItems.h"
#include "minsky.h"
#include <cairo_base.h>

#include <ecolab_epilogue.h>
using namespace std;
using namespace ecolab::cairo;
using namespace minsky;

#ifdef MAC_OSX_TK
// bizarrely, the Macintosh canvas is offset by 40 pixels (as in wasted space).
static const float yoffs=-40;
#else
static const float yoffs=0;
#endif

namespace minsky
{
  void Canvas::mouseDown(float x, float y)
  {
    y+=yoffs;
    // firstly, see if the user is selecting an item
    itemFocus=model->findAny(&Group::items,
                       [&](const ItemPtr& i){return i->visible() && i->contains(x,y);});
    if (!itemFocus)
      // check for groups
      itemFocus=model->findAny(&Group::groups,
                               [&](const GroupPtr& i){return i->contains(x,y);});

    if (itemFocus)
      {
        clickType=itemFocus->clickType(x,y);
        switch (clickType)
          {
          case ClickType::onPort:
            // items all have their output port first, if they have an output port at all.
            if ((fromPort=itemFocus->closestOutPort(x,y)))
              {
                termX=x;
                termY=y;
                itemFocus.reset();
              }
            break;
          case ClickType::onSlider:
          case ClickType::onItem:
            moveOffsX=x-itemFocus->x();
            moveOffsY=y-itemFocus->y();
            break;
          case ClickType::outside:
            itemFocus.reset();
            if (lassoMode==LassoMode::none)
              lassoMode=LassoMode::lasso;
            break;
          }
      }
    else
      {
        wireFocus=model->findAny(&Group::wires,
                       [&](const WirePtr& i){return i->near(x,y);});
        if (wireFocus)
          handleSelected=wireFocus->nearestHandle(x,y);
        else
          if (lassoMode==LassoMode::none)
            lassoMode=LassoMode::lasso;
     }

    if (lassoMode==LassoMode::lasso)
      {
        lasso.x0=x;
        lasso.y0=y;
      }
  }

  
  void Canvas::mouseUp(float x, float y)
  {
    mouseMove(x,y);
    y+=yoffs;
    if (fromPort.get())
      {
        if (auto dest=model->findAny(&Group::items,
                                     [&](const ItemPtr& i){return i->contains(x,y);}))
          if (auto to=dest->closestInPort(x,y))
            model->addWire(fromPort,to);
        fromPort.reset();
      }
    
    if (wireFocus)
      wireFocus->editHandle(handleSelected,x,y);
    
    switch (lassoMode)
      {
      case LassoMode::lasso:
        select(lasso.x0,lasso.y0,x,y);
        requestRedraw();
        break;
      case LassoMode::itemResize:
        if (item)
          {
            item->resize(lasso);
            requestRedraw();
          }
        break;
      default: break;
      }
    lassoMode=LassoMode::none;

    
    itemIndicator=false;
    itemFocus.reset();
    wireFocus.reset();
  }
  
  void Canvas::mouseMove(float x, float y)
  {
    y+=yoffs;
    if (itemFocus && clickType==ClickType::onItem)
      {
        updateRegion=LassoBox(itemFocus->x(),itemFocus->y(),x,y);
        // move item relatively to avoid accidental moves on double click
        itemFocus->moveTo(x-moveOffsX, y-moveOffsY);
        // check if the move has moved outside or into a group
        if (auto g=itemFocus->group.lock())
          if (g==model || !g->contains(itemFocus->x(),itemFocus->y()))
            {
              if (auto toGroup=model->minimalEnclosingGroup
                  (itemFocus->x(),itemFocus->y(),itemFocus->x(),itemFocus->y(),itemFocus.get()))
                {
                  auto fromGroup=itemFocus->group.lock();
                  toGroup->addItem(itemFocus);
                  toGroup->splitBoundaryCrossingWires();
                  if (fromGroup) fromGroup->splitBoundaryCrossingWires();
                }
              else
                {
                  auto fromGroup=itemFocus->group.lock();
                  model->addItem(itemFocus);
                  model->splitBoundaryCrossingWires();
                  if (fromGroup) fromGroup->splitBoundaryCrossingWires();
                }
            }
        if (auto g=itemFocus->group.lock())
          g->checkAddIORegion(itemFocus);
        requestRedraw();
      }
    else if (itemFocus && clickType==ClickType::onSlider)
      {
        if (auto v=dynamic_cast<VariableBase*>(itemFocus.get()))
          {
            RenderVariable rv(*v);
            double rw=fabs(v->zoomFactor*rv.width()*cos(v->rotation*M_PI/180));
            v->sliderSet((x-v->x()) * (v->sliderMax-v->sliderMin) /
                         rw + 0.5*(v->sliderMin+v->sliderMax));
            requestRedraw();
          }
      }
    else if (fromPort.get())
      {
        termX=x;
        termY=y;
        requestRedraw();
      }
    else if (wireFocus)
      {
        wireFocus->editHandle(handleSelected,x,y);
        requestRedraw();
      }
    else if (lassoMode==LassoMode::lasso)
      {
        lasso.x1=x;
        lasso.y1=y;
        requestRedraw();
      }
    else if (lassoMode==LassoMode::itemResize && item.get())
      {
        lasso.x1=x;
        lasso.y1=y;
        // make lasso symmetric around item's (x,y)
        lasso.x0=2*item->x() - x;
        lasso.y0=2*item->y() - y;
        requestRedraw();
      }
    else
      {
        // set mouse focus to display ports etc.
        model->recursiveDo(&Group::items, [&](Items&,Items::iterator& i)
                           {
                             bool mf=(*i)->contains(x,y);
                             if (mf!=(*i)->mouseFocus)
                               {
                                 (*i)->mouseFocus=mf;
                                 requestRedraw();
                               }        
                             return false;
                           });
        model->recursiveDo(&Group::wires, [&](Wires&,Wires::iterator& i)
                           {
                             bool mf=(*i)->near(x,y);
                             if (mf!=(*i)->mouseFocus)
                               {
                                 (*i)->mouseFocus=mf;
                                 requestRedraw();
                               }        
                             return false;
                           });
      }
  }

  void Canvas::select(const LassoBox& lasso)
  {
    selection.clear();

    auto topLevel = model->minimalEnclosingGroup(lasso.x0,lasso.y0,lasso.x1,lasso.y1);

    if (!topLevel) topLevel=&*model;

    for (auto& i: topLevel->items)
      if (i->visible() && lasso.intersects(*i))
        {
          selection.items.push_back(i);
          i->selected=true;
        }

    for (auto& i: topLevel->groups)
      if (i->visible() && lasso.intersects(*i))
        {
          selection.groups.push_back(i);
          i->selected=true;
        }

    for (auto& i: topLevel->wires)
      if (i->visible() && lasso.contains(*i))
        selection.wires.push_back(i);

    minsky().copy();
  }

  
  ItemPtr Canvas::itemAt(float x, float y)
  {
    y+=yoffs;
    auto item=model->findAny(&Group::items,
                        [&](const ItemPtr& i){return i->visible() && i->contains(x,y);});
    if (!item)
      item=model->findAny(&Group::groups,
                       [&](const ItemPtr& i){return i->visible() && i->contains(x,y);});
    return item;
  }
  
  void Canvas::getWireAt(float x, float y)
  {
    y+=yoffs;
    wire=model->findAny(&Group::wires,
                        [&](const WirePtr& i){return i->near(x,y);});
  }

  void Canvas::groupSelection()
  {
    GroupPtr r=model->addGroup(new Group);
    for (auto& i: selection.items)
      r->addItem(i);
    for (auto& i: selection.groups)
      r->addItem(i);
    r->resizeOnContents();
    r->splitBoundaryCrossingWires();
    setItemFocus(r);
  }

  
  void Canvas::removeItemFromItsGroup()
  {
    if (item)
      if (auto g=item->group.lock())
        {
          if (auto parent=g->group.lock())
            {
              itemFocus=parent->addItem(item);
              itemFocus->m_visible=true;
              g->splitBoundaryCrossingWires();
            }
          // else item already at toplevel
        }
  }

  void Canvas::selectAllVariables()
  {
    selection.clear();
    auto var=dynamic_cast<VariableBase*>(item.get());
    if (!var)
      if (auto i=dynamic_cast<IntOp*>(item.get()))
        var=i->intVar.get();
    if (var)
      {
        model->recursiveDo
          (&GroupItems::items, [&](const Items&,Items::const_iterator i)
           {
             if (auto v=dynamic_cast<VariableBase*>(i->get()))
               if (v->valueId()==var->valueId())
                 {
                   selection.items.push_back(*i);
                   v->selected=true;
                 }
             return false;
           });
       }
  }

  void Canvas::renameAllInstances(const string newName)
  {
    auto var=dynamic_cast<VariableBase*>(item.get());
    if (!var)
      if (auto i=dynamic_cast<IntOp*>(item.get()))
        var=i->intVar.get();
    if (var)
      {
        auto valueId=var->valueId();
        model->recursiveDo
          (&GroupItems::items, [&](Items&,Items::iterator i)
           {
             if (auto v=dynamic_cast<VariableBase*>(i->get()))
               if (v->valueId()==valueId)
                 v->name(newName);
             return false;
           });
       }
   }
  
  void Canvas::ungroupItem()
  {
    if (auto g=dynamic_cast<Group*>(item.get()))
      {
        if (auto p=g->group.lock())
          {
            p->moveContents(*g);
            deleteItem();
          }
        // else item is toplevel which can't be ungrouped
      }
  }


  void Canvas::copyItem()
  {
    if (item)
      {
        ItemPtr newItem;
        // cannot duplicate an integral, just its variable
        if (auto intop=dynamic_cast<IntOp*>(item.get()))
          newItem.reset(intop->intVar->clone());
        else if (auto group=dynamic_cast<Group*>(item.get()))
          newItem=group->copy();
        else
          newItem.reset(item->clone());
        setItemFocus(model->addItem(newItem));
        model->normaliseGroupRefs(model);
        newItem->m_visible=true;
      }
  }

  void Canvas::openGroupInCanvas(const ItemPtr& item)
  {
    if (auto g=dynamic_pointer_cast<Group>(item))
      {
        if (auto parent=model->group.lock())
          model->setZoom(parent->zoomFactor);
        model=g;
        float zoomFactor=1.1*model->displayZoom;
        if (!model->displayContents())
          {
            // we need to move the io variables
            for (auto& v: model->inVariables)
              {
                float x=v->x(), y=v->y();
                zoom(x,model->x(),zoomFactor);
                zoom(y,model->y(),zoomFactor);
                v->moveTo(x,y);
              }
            for (auto& v: model->outVariables)
              {
                float x=v->x(), y=v->y();
                zoom(x,model->x(),zoomFactor);
                zoom(y,model->y(),zoomFactor);
                v->moveTo(x,y);
              }
          }
        model->zoom(model->x(),model->y(),zoomFactor);
        requestRedraw();
      }
  }

  void Canvas::copyVars(const std::vector<VariablePtr>& v)
  {
    auto group=model->addGroup(new Group);
    setItemFocus(group);
    float maxWidth=0, totalHeight=0;
    vector<float> widths, heights;
    for (auto i: v)
      {
        RenderVariable rv(*i);
        widths.push_back(rv.width());
        heights.push_back(rv.height());
        maxWidth=max(maxWidth, widths.back());
        totalHeight+=heights.back();
      }
    float y=group->y() - totalHeight;
    for (size_t i=0; i<v.size(); ++i)
      {
        auto ni=v[i]->clone();
        group->addItem(ni);
        ni->rotation=0;
        ni->moveTo(maxWidth - widths[i],
                   y+heights[i]);
        // variables need to refer to outer scope
        if (ni->name()[0]!=':')
          ni->name(':'+ni->name());
        y+=2*heights[i];
      }
  }
  
  
  void Canvas::handleArrows(int dir, float x, float y)
  {
    if (itemAt(x,y)->handleArrows(dir))
      requestRedraw();
  }
  
  void Canvas::zoomToDisplay()
  {
    if (auto g=dynamic_cast<Group*>(item.get()))
      model->zoom(g->x(),g->y(),1.1*g->displayZoom);
  }

  bool Canvas::selectVar(float x, float y) 
  {
    y+=yoffs;
    if (item)
      {
        if (auto v=item->select(x,y))
          {
            item=v;
            return true;
          }
      }
    return false;
  }
    
  bool Canvas::findVariableDefinition()
  {
    if (auto iv=dynamic_cast<VariableBase*>(item.get()))
      {
        if (iv->type()==VariableType::constant ||
            iv->type()==VariableType::parameter || iv->inputWired())
          return true;
        
        auto def=model->findAny
          (&GroupItems::items, [&](const ItemPtr& i) {
            if (auto v=dynamic_cast<VariableBase*>(i.get()))
              return v->inputWired() && v->valueId()==iv->valueId();
            else if (auto g=dynamic_cast<GodleyIcon*>(i.get()))
              for (auto& v: g->stockVars)
                {
                  if (v->valueId()==iv->valueId())
                    return true;
                }
            else if (auto o=dynamic_cast<IntOp*>(i.get()))
              return o->intVar->valueId()==iv->valueId();
            return false;
          });
        if (def)
          item=def;
        return def.get();
      }
    return false;
  }

  void Canvas::redraw(int x0, int y0, int width, int height)
  {
    updateRegion.x0=x0;
    updateRegion.y0=y0;
    updateRegion.x1=x0+width;
    updateRegion.y1=y0+height;
    redrawUpdateRegion();
  }
  
  void Canvas::redraw()
  {
    updateRegion.x0=-numeric_limits<float>::max();
    updateRegion.y0=-numeric_limits<float>::max();
    updateRegion.x1=numeric_limits<float>::max();
    updateRegion.y1=numeric_limits<float>::max();
    redrawUpdateRegion();
  }

  void Canvas::redrawUpdateRegion()
  {
    if (!surface.get()) return;
    auto cairo=surface->cairo();
    cairo_set_line_width(cairo, 1);
    // items
    model->recursiveDo
      (&GroupItems::items, [&](const Items&, Items::const_iterator i)
       {
         auto& it=**i;
         it.setCairoSurface(surface);
         if (it.visible() && updateRegion.intersects(it))
           {
             cairo_save(cairo);
             cairo_identity_matrix(cairo);
             cairo_translate(cairo,it.x(), it.y());
             it.draw(cairo);
             cairo_restore(cairo);
           }
         return false;
       });

    // groups
    model->recursiveDo
      (&GroupItems::groups, [&](const Groups&, Groups::const_iterator i)
       {
         auto& it=**i;
         if (it.visible() && updateRegion.intersects(it))
           {
             cairo_save(cairo);
             cairo_identity_matrix(cairo);
             cairo_translate(cairo,it.x(), it.y());
             it.draw(cairo);
             cairo_restore(cairo);
           }
         return false;
       });

    // draw all wires - wires will go over the top of any icons. TODO
    // introduce an ordering concept if needed
    model->recursiveDo
      (&GroupItems::wires, [&](const Wires&, Wires::const_iterator i)
       {
         const Wire& w=**i;
         if (w.visible() && updateRegion.intersects(w))
           w.draw(cairo);
         return false;
       });

    if (fromPort.get()) // we're in process of creating a wire
      {
        cairo_move_to(cairo,fromPort->x(),fromPort->y());
        cairo_line_to(cairo,termX,termY);
        cairo_stroke(cairo);
        // draw arrow
        cairo_save(cairo);
        cairo_translate(cairo, termX,termY);
        cairo_rotate(cairo,atan2(termY-fromPort->y(), termX-fromPort->x()));
        cairo_move_to(cairo,0,0);
        cairo_line_to(cairo,-5,-3); 
        cairo_line_to(cairo,-3,0); 
        cairo_line_to(cairo,-5,3);
        cairo_close_path(cairo);
        cairo_fill(cairo);
        cairo_restore(cairo);
      }

    if (lassoMode!=LassoMode::none)
      {
        cairo_rectangle(cairo,lasso.x0,lasso.y0,lasso.x1-lasso.x0,lasso.y1-lasso.y0);
        cairo_stroke(cairo);
      }

    if (itemIndicator) // draw a red circle to indicate an error or other marker
      {
        cairo_save(surface->cairo());
        cairo_set_source_rgb(surface->cairo(),1,0,0);
        cairo_arc(surface->cairo(),item->x(),item->y(),15,0,2*M_PI);
        cairo_stroke(surface->cairo());
        cairo_restore(surface->cairo());
      }
    
    surface->blit();
  }

  void Canvas::recentre()
  {
    SurfacePtr tmp(surface);
    surface.reset(new Surface(cairo_recording_surface_create(CAIRO_CONTENT_COLOR,nullptr)));
    redraw();
    model->moveTo(model->x()-surface->left(), model->y()-surface->top());
    surface=tmp;
    requestRedraw();
  }
}
