#include "ui/menu.h"
#include <utility>

namespace autoaimer {
MenuStack::MenuStack(std::unique_ptr<Menu> root) {
    pages[0] = std::move(root);
}

bool MenuStack::apply(MenuAction action) {
    switch (action.kind) {
        case MenuAction::Stay:
            return true;
        case MenuAction::Push:
            if (!action.page || depth == pages.size())
                return false;
            pages[depth++] = std::move(action.page);
            return true;
        case MenuAction::Pop:
            if (depth == 1)
                return false;
            pages[--depth].reset();
            return true;
    }
    return false;
}
} // namespace autoaimer
