// Aseprite UI Library
// Copyright (C) 2001-2013, 2015  David Capello
// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include "base/signal.h"
#include "gfx/rect.h"
#include "ui/register_message.h"
#include "ui/separator.h"
#include "ui/widget.h"

#include <memory>

namespace ui {

  class MenuItem;
  class Timer;
  struct MenuBaseData;

  class Menu : public Widget {
  public:
    Menu();
    ~Menu();

    void showPopup(const gfx::Point& pos);

    // Returns the MenuItem that has as submenu this menu.
    MenuItem* getOwnerMenuItem() {
      return m_menuitem;
    }

    // True when this menu's items don't fit in the available height
    // and must be scrolled vertically.
    bool isScrollable() const { return m_scrollable; }
    bool canScrollUp() const { return m_scrollable && m_scrollTopIndex > 0; }
    bool canScrollDown() const { return m_scrollable && m_hasMoreBelow; }

    // Bounds (in screen coordinates) of the scroll indicators, empty
    // when scrolling in that direction isn't possible/needed.
    const gfx::Rect& scrollUpBounds() const { return m_scrollUpBounds; }
    const gfx::Rect& scrollDownBounds() const { return m_scrollDownBounds; }

  protected:
    virtual void onPaint(PaintEvent& ev) override;
    virtual void onResize(ResizeEvent& ev) override;
    virtual void onSizeHint(SizeHintEvent& ev) override;
    virtual bool onProcessMessage(Message* msg) override;

  private:
    void setOwnerMenuItem(MenuItem* ownerMenuItem) {
      m_menuitem = ownerMenuItem;
    }

    void closeAll();

    MenuItem* getHighlightedItem();
    void highlightItem(MenuItem* menuitem, bool click, bool open_submenu, bool select_first_child);
    void unhighlightItem();

    void layoutItems();
    void scrollBy(int itemDelta);
    void ensureVisible(Widget* item);
    void startAutoScroll(int direction);
    void stopAutoScroll();

    MenuItem* m_menuitem;         // From where the menu was open

    // Vertical scrolling state (only used for popup/submenu menus,
    // never for the top-level menu-bar).
    bool m_scrollable;
    bool m_hasMoreBelow;
    int m_scrollTopIndex;
    gfx::Rect m_scrollUpBounds;
    gfx::Rect m_scrollDownBounds;

    // While the mouse hovers over one of the scroll arrows, this timer
    // repeatedly scrolls the menu (-1 = up, 0 = not auto-scrolling, 1 = down).
    std::unique_ptr<Timer> m_scrollTimer;
    int m_autoScrollDirection;

    friend class MenuBox;
    friend class MenuItem;
  };

  class MenuBox : public Widget {
  public:
    MenuBox(WidgetType type = kMenuBoxWidget);
    ~MenuBox();

    Menu* getMenu();
    void setMenu(Menu* menu);

    MenuBaseData* getBase() {
      return m_base;
    }

    // Closes all menu-boxes and goes back to the normal state of the
    // menu-bar.
    void cancelMenuLoop();

  protected:
    virtual bool onProcessMessage(Message* msg) override;
    virtual void onResize(ResizeEvent& ev) override;
    virtual void onSizeHint(SizeHintEvent& ev) override;
    MenuBaseData* createBase();

  private:
    void closePopup();

    MenuBaseData* m_base;

    friend class Menu;
  };

  class MenuBar : public MenuBox {
  public:
    MenuBar();

    static bool expandOnMouseover();
    static void setExpandOnMouseover(bool state);

  private:
    static bool m_expandOnMouseover;
  };

  class MenuItem : public Widget {
  public:
    MenuItem(const std::string& text);
    ~MenuItem();

    Menu* getSubmenu();
    void setSubmenu(Menu* submenu);

    bool isHighlighted() const;
    void setHighlighted(bool state);

    // Returns true if the MenuItem has a submenu.
    bool hasSubmenu() const;

    // Returns true if the submenu is opened.
    bool hasSubmenuOpened() const {
      return (m_submenu_menubox != NULL);
    }

    // Returns the menu-box where the sub-menu has been opened, or just
    // NULL if the sub-menu is closed.
    MenuBox* getSubmenuContainer() const {
      return m_submenu_menubox;
    }

    // Fired when the menu item is clicked.
    base::Signal0<void> Click;

  protected:
    virtual bool onProcessMessage(Message* msg) override;
    virtual void onPaint(PaintEvent& ev) override;
    virtual void onSizeHint(SizeHintEvent& ev) override;
    virtual void onClick();

    bool inBar();

  private:
    void openSubmenu(bool select_first);
    void closeSubmenu(bool last_of_close_chain);
    void startTimer();
    void stopTimer();
    void executeClick();

    bool m_highlighted;           // Is it highlighted?
    Menu* m_submenu;              // The sub-menu
    MenuBox* m_submenu_menubox;   // The opened menubox for this menu-item
    std::unique_ptr<Timer> m_submenu_timer; // Timer to open the submenu

    friend class Menu;
    friend class MenuBox;
  };

  class MenuSeparator : public Separator {
  public:
    MenuSeparator() : Separator("", HORIZONTAL) {
    }
  };

  extern RegisterMessage kOpenMenuItemMessage;
  extern RegisterMessage kCloseMenuItemMessage;
  extern RegisterMessage kClosePopupMessage;
  extern RegisterMessage kExecuteMenuItemMessage;

} // namespace ui
