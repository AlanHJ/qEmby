#include "contextmenuutils.h"

#include "../components/elidedlabel.h"
#include "../managers/thememanager.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSize>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QWidget>

#include <memory>

namespace {

enum class TextContextActionRole {
    None,
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    Delete,
    SelectAll,
};

QWidget* resolveTextContextWidget(QObject* watched) {
    QObject* current = watched;
    while (current) {
        if (auto* lineEdit = qobject_cast<QLineEdit*>(current)) {
            return lineEdit;
        }
        if (auto* textEdit = qobject_cast<QTextEdit*>(current)) {
            return textEdit;
        }
        if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(current)) {
            return plainTextEdit;
        }
        current = current->parent();
    }
    return nullptr;
}

QLabel* resolveCopyableLabelWidget(QObject* watched) {
    QObject* current = watched;
    while (current) {
        if (auto* label = qobject_cast<QLabel*>(current)) {
            return label;
        }
        current = current->parent();
    }
    return nullptr;
}

QMenu* createStandardTextMenu(QWidget* widget) {
    if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
        return lineEdit->createStandardContextMenu();
    }
    if (auto* textEdit = qobject_cast<QTextEdit*>(widget)) {
        return textEdit->createStandardContextMenu();
    }
    if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(widget)) {
        return plainTextEdit->createStandardContextMenu();
    }
    return nullptr;
}

bool matchesStandardShortcut(const QAction* action, QKeySequence::StandardKey standardKey) {
    if (!action) {
        return false;
    }

    QList<QKeySequence> actionShortcuts = action->shortcuts();
    if (actionShortcuts.isEmpty() && !action->shortcut().isEmpty()) {
        actionShortcuts.append(action->shortcut());
    }

    const QList<QKeySequence> expectedBindings = QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& actionShortcut : actionShortcuts) {
        for (const QKeySequence& expectedBinding : expectedBindings) {
            if (actionShortcut.matches(expectedBinding) == QKeySequence::ExactMatch) {
                return true;
            }
        }
    }

    return false;
}

QString normalizeActionText(QString text) {
    const int shortcutSeparatorIndex = text.indexOf(QLatin1Char('\t'));
    if (shortcutSeparatorIndex >= 0) {
        text = text.left(shortcutSeparatorIndex);
    }

    static const QRegularExpression acceleratorPattern(QStringLiteral(R"(\s*\(&.\))"));
    static const QRegularExpression trailingParenPattern(QStringLiteral(R"(\s*\([^)]+\)$)"));

    text.remove(acceleratorPattern);
    text.remove(trailingParenPattern);
    text.remove(QLatin1Char('&'));
    text.remove(QStringLiteral("..."));
    text.remove(QChar(0x2026));
    return text.simplified().toLower();
}

TextContextActionRole classifyAction(const QAction* action) {
    if (!action || action->isSeparator()) {
        return TextContextActionRole::None;
    }

    if (matchesStandardShortcut(action, QKeySequence::Undo)) {
        return TextContextActionRole::Undo;
    }
    if (matchesStandardShortcut(action, QKeySequence::Redo)) {
        return TextContextActionRole::Redo;
    }
    if (matchesStandardShortcut(action, QKeySequence::Cut)) {
        return TextContextActionRole::Cut;
    }
    if (matchesStandardShortcut(action, QKeySequence::Copy)) {
        return TextContextActionRole::Copy;
    }
    if (matchesStandardShortcut(action, QKeySequence::Paste)) {
        return TextContextActionRole::Paste;
    }
    if (matchesStandardShortcut(action, QKeySequence::SelectAll)) {
        return TextContextActionRole::SelectAll;
    }

    const QString normalizedText = normalizeActionText(action->text());
    if (normalizedText == QStringLiteral("undo") || normalizedText.contains(QStringLiteral("undo")) ||
        normalizedText == QStringLiteral("撤销") || normalizedText.contains(QStringLiteral("撤销"))) {
        return TextContextActionRole::Undo;
    }
    if (normalizedText == QStringLiteral("redo") || normalizedText.contains(QStringLiteral("redo")) ||
        normalizedText == QStringLiteral("重做") || normalizedText.contains(QStringLiteral("重做"))) {
        return TextContextActionRole::Redo;
    }
    if (normalizedText == QStringLiteral("cut") || normalizedText.contains(QStringLiteral("cut")) ||
        normalizedText == QStringLiteral("剪切") || normalizedText.contains(QStringLiteral("剪切"))) {
        return TextContextActionRole::Cut;
    }
    if (normalizedText == QStringLiteral("copy") || normalizedText.contains(QStringLiteral("copy")) ||
        normalizedText == QStringLiteral("复制") || normalizedText.contains(QStringLiteral("复制"))) {
        return TextContextActionRole::Copy;
    }
    if (normalizedText == QStringLiteral("paste") || normalizedText.contains(QStringLiteral("paste")) ||
        normalizedText == QStringLiteral("粘贴") || normalizedText.contains(QStringLiteral("粘贴"))) {
        return TextContextActionRole::Paste;
    }
    if (normalizedText == QStringLiteral("delete") || normalizedText.contains(QStringLiteral("delete")) ||
        normalizedText == QStringLiteral("删除") || normalizedText.contains(QStringLiteral("删除"))) {
        return TextContextActionRole::Delete;
    }
    if (normalizedText == QStringLiteral("select all") || normalizedText.contains(QStringLiteral("select all")) ||
        normalizedText == QStringLiteral("全选") || normalizedText.contains(QStringLiteral("全选"))) {
        return TextContextActionRole::SelectAll;
    }

    return TextContextActionRole::None;
}

QColor menuIconColor(bool enabled) {
    if (!enabled) {
        return ThemeManager::instance()->isDarkMode() ? QColor(QStringLiteral("#64748B"))
                                                      : QColor(QStringLiteral("#94A3B8"));
    }
    return ThemeManager::instance()->isDarkMode() ? QColor(QStringLiteral("#CBD5E1"))
                                                  : QColor(QStringLiteral("#475569"));
}

QIcon iconForActionRole(TextContextActionRole role, const QColor& color) {
    switch (role) {
    case TextContextActionRole::Undo:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/context-undo.svg"), color);
    case TextContextActionRole::Redo:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/context-redo.svg"), color);
    case TextContextActionRole::Cut:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/context-cut.svg"), color);
    case TextContextActionRole::Copy:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/context-copy.svg"), color);
    case TextContextActionRole::Paste:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/context-paste.svg"), color);
    case TextContextActionRole::Delete:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/delete.svg"), color);
    case TextContextActionRole::SelectAll:
        return ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/context-select-all.svg"), color);
    case TextContextActionRole::None:
        break;
    }

    return {};
}

QString translatedTextForRole(TextContextActionRole role) {
    switch (role) {
    case TextContextActionRole::Undo:
        return QCoreApplication::translate("ContextMenuUtils", "Undo");
    case TextContextActionRole::Redo:
        return QCoreApplication::translate("ContextMenuUtils", "Redo");
    case TextContextActionRole::Cut:
        return QCoreApplication::translate("ContextMenuUtils", "Cut");
    case TextContextActionRole::Copy:
        return QCoreApplication::translate("ContextMenuUtils", "Copy");
    case TextContextActionRole::Paste:
        return QCoreApplication::translate("ContextMenuUtils", "Paste");
    case TextContextActionRole::Delete:
        return QCoreApplication::translate("ContextMenuUtils", "Delete");
    case TextContextActionRole::SelectAll:
        return QCoreApplication::translate("ContextMenuUtils", "Select All");
    case TextContextActionRole::None:
        break;
    }

    return {};
}

QString resolvedLabelCopyText(const QLabel* label) {
    if (!label) {
        return {};
    }

    if (label->hasSelectedText()) {
        return label->selectedText();
    }

    if (const auto* elidedLabel = qobject_cast<const ElidedLabel*>(label)) {
        if (!elidedLabel->fullText().trimmed().isEmpty()) {
            return elidedLabel->fullText();
        }
    }

    const QString rawText = label->text();
    if (rawText.trimmed().isEmpty()) {
        return {};
    }

    const bool isRichText = label->textFormat() == Qt::RichText ||
                            (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(rawText));
    return isRichText ? QTextDocumentFragment::fromHtml(rawText).toPlainText() : rawText;
}

void applyMenuPresentation(QMenu* menu) {
    if (!menu) {
        return;
    }

    QList<QAction*> actionableItems;
    QList<TextContextActionRole> resolvedRoles;
    int classifiedCount = 0;

    for (QAction* action : menu->actions()) {
        if (!action || action->isSeparator()) {
            continue;
        }

        actionableItems.append(action);
        const TextContextActionRole role = classifyAction(action);
        resolvedRoles.append(role);
        if (role != TextContextActionRole::None) {
            ++classifiedCount;
        }
    }

    if (classifiedCount == 0) {
        static const QList<TextContextActionRole> fallbackRoles = {
            TextContextActionRole::Undo,      TextContextActionRole::Redo,   TextContextActionRole::Cut,
            TextContextActionRole::Copy,      TextContextActionRole::Paste,  TextContextActionRole::Delete,
            TextContextActionRole::SelectAll,
        };

        for (int i = 0; i < actionableItems.size() && i < fallbackRoles.size(); ++i) {
            resolvedRoles[i] = fallbackRoles.at(i);
        }
    }

    for (int i = 0; i < actionableItems.size(); ++i) {
        QAction* action = actionableItems.at(i);
        const TextContextActionRole role = resolvedRoles.at(i);
        if (role == TextContextActionRole::None) {
            continue;
        }

        action->setShortcutVisibleInContextMenu(true);
        action->setIconVisibleInMenu(true);
        action->setText(translatedTextForRole(role));
        action->setIcon(iconForActionRole(role, menuIconColor(action->isEnabled())));
    }
}

QPoint resolveMenuPosition(QWidget* widget, const QContextMenuEvent* event) {
    if (!widget || !event) {
        return {};
    }

    if (!event->globalPos().isNull()) {
        return event->globalPos();
    }

    const QPoint localPos = event->pos();
    if (widget->rect().contains(localPos)) {
        return widget->mapToGlobal(localPos);
    }

    return widget->mapToGlobal(widget->rect().center());
}

std::unique_ptr<QMenu> createLabelContextMenu(QWidget* parent, const QString& copyText) {
    auto menu = std::make_unique<QMenu>(parent);
    auto* copyAction = menu->addAction(translatedTextForRole(TextContextActionRole::Copy));
    copyAction->setShortcutVisibleInContextMenu(true);
    copyAction->setIconVisibleInMenu(true);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setIcon(iconForActionRole(TextContextActionRole::Copy, menuIconColor(true)));

    QObject::connect(copyAction, &QAction::triggered, menu.get(), [copyText]() {
        if (auto* clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(copyText);
        }
    });

    return menu;
}

} 

namespace ContextMenuUtils {

bool showStyledTextContextMenu(QObject* watched, QEvent* event) {
    if (!watched || !event || event->type() != QEvent::ContextMenu) {
        return false;
    }

    QWidget* targetWidget = resolveTextContextWidget(watched);
    if (!targetWidget) {
        return false;
    }

    auto* contextEvent = static_cast<QContextMenuEvent*>(event);
    std::unique_ptr<QMenu> menu(createStandardTextMenu(targetWidget));
    if (!menu || menu->actions().isEmpty()) {
        return false;
    }

    menu->setObjectName(QStringLiteral("AppTextContextMenu"));
    menu->setAttribute(Qt::WA_StyledBackground, true);
    menu->setSeparatorsCollapsible(true);
    menu->ensurePolished();
    applyMenuPresentation(menu.get());
    menu->exec(resolveMenuPosition(targetWidget, contextEvent));
    return true;
}

bool showStyledLabelContextMenu(QObject* watched, QEvent* event) {
    if (!watched || !event || event->type() != QEvent::ContextMenu) {
        return false;
    }

    QLabel* label = resolveCopyableLabelWidget(watched);
    if (!label || label->contextMenuPolicy() != Qt::DefaultContextMenu) {
        return false;
    }

    const QString copyText = resolvedLabelCopyText(label);
    if (copyText.trimmed().isEmpty()) {
        return false;
    }

    auto* contextEvent = static_cast<QContextMenuEvent*>(event);
    std::unique_ptr<QMenu> menu = createLabelContextMenu(label, copyText);
    menu->setObjectName(QStringLiteral("AppTextContextMenu"));
    menu->setAttribute(Qt::WA_StyledBackground, true);
    menu->setSeparatorsCollapsible(true);
    menu->ensurePolished();
    menu->exec(resolveMenuPosition(label, contextEvent));
    return true;
}

} 
