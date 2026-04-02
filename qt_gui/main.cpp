#include "mainwindow.h"
#include <QApplication>
#include <QFont>
#include <QResource>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 注册字体资源
    QResource::registerResource("/font/SourceHanSansCN-Regular.ttf");
    QResource::registerResource("/font/NotoColorEmoji.ttf");

    // 加载字体
    int fontId = QFontDatabase::addApplicationFont(":/font/SourceHanSansCN-Regular.ttf");
    QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!fontFamilies.isEmpty()) {
        QFont defaultFont(fontFamilies.at(0), 10);
        a.setFont(defaultFont);
    }

    // 加载 NotoColorEmoji 字体（用于显示表情）
    int emojiFontId = QFontDatabase::addApplicationFont(":/font/NotoColorEmoji.ttf");
    QStringList emojiFontFamilies = QFontDatabase::applicationFontFamilies(emojiFontId);
    if (!emojiFontFamilies.isEmpty()) {
        // 设置 Emoji 字体为全局替换字体，确保表情优先使用彩色字体
        QFont::insertSubstitution("Noto Color Emoji", emojiFontFamilies.at(0));
    }

    MainWindow w;
    w.show();
    return a.exec();
}
