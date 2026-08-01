//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Layout manager implementation
//                                              (C)2005-2006, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "layout.h"

using namespace _GUI_;

// --- LayoutManager ---

bool isVisible(Control* control)
{
   return (control && control->isVisible());
}

void adjustVertical(size_t width, size_t& height, Control* control)
{
   if (isVisible(control)) {
	  control->setWidth(width);
      if (height > control->getHeight() + 4) {
         height -= control->getHeight();
      }
      else {
	     control->setHeight(height - 4);
	     height = 4;
      }
   }
}

void adjustHorizontal(size_t& width, size_t height, Control* control)
{
   if (isVisible(control)) {
      control->setHeight(height);
      if (width > control->getWidth() + 4) {
         width -= control->getWidth();
      }
      else {
	     control->setWidth(width - 4);
	     width = 4;
      }
   }
}

void adjustClient(size_t width, size_t height, Control* control)
{
   if (isVisible(control)) {
      control->setWidth(width);
	  control->setHeight(height);
   }
}

void LayoutManager :: resizeTo(_GUI_::Rectangle area)
{
   size_t totalHeight = area.Height();
   size_t totalWidth = area.Width();
   int y = area.topLeft.x;
   int x = area.topLeft.y;

   adjustVertical(totalWidth, totalHeight, _top);
   adjustVertical(totalWidth, totalHeight, _bottom);
   adjustHorizontal(totalWidth, totalHeight, _left);
   adjustHorizontal(totalWidth, totalHeight, _right);
   adjustClient(totalWidth, totalHeight, _client);

   if (isVisible(_top)) {
	  _top->setCoordinate(area.topLeft.x, area.topLeft.y);
	  _top->resize();
	  y += _top->getHeight();
   }
   if (isVisible(_bottom)) {
	  _bottom->setCoordinate(area.topLeft.x, y + totalHeight);
	  _bottom->resize();
   }
   if (isVisible(_left)) {
      _left->setCoordinate(area.topLeft.x, y);
	  _left->resize();
	  x += _left->getWidth();
   }
   if (isVisible(_right)) {
      _right->setCoordinate(area.topLeft.x, y);
	  _right->resize();
   }
   if (isVisible(_client)) {
      _client->setCoordinate(x, y);
	  _client->resize();
   }
}
