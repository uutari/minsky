/*
  @copyright Steve Keen 2015
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
#include "cairoItems.h"
#include "switchIcon.h"
#include "minsky.h"
#include "minsky_epilogue.h"
using namespace ecolab::cairo;
using namespace ecolab;
using namespace std;

namespace minsky
{
  SwitchIcon::SwitchIcon()
  {
    ports.emplace_back(new Port(*this,Port::noFlags));
    ports.emplace_back(new Port(*this, Port::inputPort));
    setNumCases(2); ///<default to if/then
  }
  
  ClickType::Type SwitchIcon::clickType(float x, float y)
  {
    double dx=x-this->x(), dy=y-this->y();
    auto z=zoomFactor();
    // check if (x,y) is within portradius of the 4 corners
    //if (fabs(fabs(dx)-iWidth()) < portRadius*z &&
    //    fabs(fabs(dy)-iWidth()) < portRadius*z &&
    //    fabs(hypot(dx,dy)-hypot(iWidth(),iWidth())) < portRadius*z)
    if (fabs(x-right()) < portRadius*z && fabs(y-bottom()*z) < portRadius*z)
      return ClickType::onResize;
    return Item::clickType(x,y);
  }  

  void SwitchIcon::setNumCases(unsigned n)
  {
    if (n<2) throw error("switches need at least two cases");
    for (unsigned i=ports.size(); i<n+2; ++i)
      ports.emplace_back(new Port(*this, Port::inputPort));
    ports.resize(n+2); // in case ports was larger than n+2
  }

  unsigned SwitchIcon::switchValue() const
  {
    double x=ports[1]->value();
    if (x<1)
      return 0;
    else if (x>=numCases()-1)
      return numCases()-1;
    else
      return unsigned(x);
  }

  Units SwitchIcon::units(bool check) const 
  {
    bool inputFound=false;
    Units r;
    for (size_t i=2; i<ports.size(); ++i)
      for (auto w: ports[i]->wires())
        if (check && inputFound)
          {
            auto tmp=w->units(check);
            if (tmp!=r)
              throw_error("incompatible units: "+tmp.str()+"≠"+r.str());
          }
        else
          {
            inputFound=true;
            r=w->units(check);
          }
    return r;
  }
  
namespace
{
  void drawResizeHandle(cairo_t* cairo, double x, double y, double sf, double angle)
  {
    cairo::CairoSave cs(cairo);
    cairo_translate(cairo,x,y);
    cairo_rotate(cairo,angle);
    cairo_scale(cairo,sf,sf);
    cairo_move_to(cairo,-1,-.2);
    cairo_line_to(cairo,-1,-1);
    cairo_line_to(cairo,1,1);
    cairo_line_to(cairo,1,0.2);
    cairo_move_to(cairo,-1,-1);
    cairo_line_to(cairo,-.2,-1);
    cairo_move_to(cairo,.2,1);
    cairo_line_to(cairo,1,1);
  }
}  
  
  void SwitchIcon::drawResizeHandles(cairo_t* cairo) const
  {
    double sf=portRadius*zoomFactor();  
    //drawResizeHandle(cairo,right()-x(),top()-y(),sf,0.5*M_PI);
    //drawResizeHandle(cairo,left()-x(),top()-y(),sf,M_PI);
    //drawResizeHandle(cairo,left()-x(),bottom()-y(),sf,1.5*M_PI);
    drawResizeHandle(cairo,right()-x(),bottom()-y(),sf,0);
    cairo_stroke(cairo);
  }    

  void SwitchIcon::draw(cairo_t* cairo) const
  {
    cairo_set_line_width(cairo,1);
    // square icon
    float width=8*zoomFactor()*numCases();
    if (width<iWidth()*zoomFactor()*numCases()) {
		width=iWidth()*zoomFactor()*numCases();
	}
    cairo_rectangle(cairo,-0.5*width,-0.5*width,width,width);
    cairo_stroke(cairo);


    float w=flipped? -width: width;
    float o=flipped? -8: 8;
    // output port
    drawTriangle(cairo, 0.5*w, 0, palette[0], flipped? M_PI: 0);
    ports[0]->moveTo(x()+0.5*w, y());
    // control port
    drawTriangle(cairo, 0, -0.5*width-8, palette[0], M_PI/2);
    ports[1]->moveTo(x(), y()-0.5*width-8);
    float dy=width/numCases();
    float y1=-0.5*width+0.5*dy;
    // case ports
    for (size_t i=2; i<ports.size(); ++i, y1+=dy)
      {
        drawTriangle(cairo, -0.5*w-o, y1, palette[(i-2)%paletteSz], flipped? M_PI: 0);
        ports[i]->moveTo(x()+-0.5*w-o, y()+y1);
      }
    // draw indicating arrow
    cairo_move_to(cairo,0.5*w, 0);
    try
      {
        y1=-0.5*width+0.5*dy+switchValue()*dy;
      }
    catch (const std::exception&)
      {
        y1=-0.5*width+0.5*dy;
      }
    cairo_line_to(cairo,-0.45*w,0.9*y1);
    cairo_stroke(cairo);

    if (mouseFocus)
      {
        drawPorts(cairo);
        displayTooltip(cairo,tooltip);
        if (onResizeHandles) drawResizeHandles(cairo);
      }

    // add 8 pt margin to allow for ports
    cairo_rectangle(cairo,-0.5*width-8,-0.5*width-8,width+16,width+8);
    cairo_clip(cairo);
     if (selected) drawSelected(cairo);
  }
  
  void SwitchIcon::resize(const LassoBox& b)
  {
    float invZ=1/zoomFactor();
    //moveTo(0.5*(b.x0+b.x1), 0.5*(b.x0+b.x1));    
    iWidth(0.5*abs(b.x1-b.x0)*invZ);
    bb.update(*this);
  }  


}
