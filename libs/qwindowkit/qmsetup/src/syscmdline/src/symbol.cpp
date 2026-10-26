#include "symbol.h"
#include "symbol_p.h"

#include <utility>

namespace SysCmdLine {

    SymbolPrivate::SymbolPrivate(Symbol::SymbolType type, std::string desc)
        : type(type), desc(std::move(desc)) {
    }

     
    Symbol::SymbolType Symbol::type() const {
        Q_D2(Symbol);
        return d->type;
    }

     
    std::string Symbol::description() const {
        Q_D2(Symbol);
        return d->desc;
    }

     
    void Symbol::setDescription(const std::string &desc) {
        Q_D(Symbol);
        d->desc = desc;
    }

     
    Symbol::HelpProvider Symbol::helpProvider() const {
        Q_D2(Symbol);
        return d->helpProvider;
    }

     
    void Symbol::setHelpProvider(const Symbol::HelpProvider &helpProvider) {
        Q_D(Symbol);
        d->helpProvider = helpProvider;
    }

     
    std::string Symbol::helpText(Symbol::HelpPosition pos, int displayOptions, void *extra) const {
        Q_D2(Symbol);
        if (d->helpProvider)
            return d->helpProvider(this, pos, displayOptions, extra);
        return {};
    }

    Symbol::Symbol(SymbolPrivate *d) : SharedBase(d) {
    }

}