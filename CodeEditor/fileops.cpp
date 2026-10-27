#include "fileops.h"
#include "theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace FileOps {

// ---------------------------------------------------------------------------
//  名字生成 / 校验
// ---------------------------------------------------------------------------
QString uniquePath(const QString &dirPath, const QString &baseName,
                   const QString &suffix)
{
    const QDir dir(dirPath);

    // 有扩展名的拆成 stem + ext，序号插在中间：main_2.cpp
    QString stem = baseName;
    QString ext  = suffix;
    if (ext.isEmpty()) {
        const QFileInfo probe(baseName);
        ext  = probe.suffix().isEmpty() ? QString()
                                        : QStringLiteral(".") + probe.suffix();
        stem = probe.suffix().isEmpty() ? baseName
                                        : baseName.left(baseName.size() - ext.size());
    }
    if (ext == baseName)          // 形如 ".gitignore"，整体当主干
        ext.clear();

    QString candidate = stem + ext;
    for (int i = 2; dir.exists(candidate) && i < 10000; ++i)
        candidate = QStringLiteral("%1 %2%3").arg(stem).arg(i).arg(ext);

    return dir.absoluteFilePath(candidate);
}

int baseNameLength(const QString &fileName)
{
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    // 隐藏文件（.gitignore）或没有点：整体当主干
    if (dot <= 0)
        return fileName.size();
    return dot;
}

bool isValidName(const QString &name, QString *why)
{
    auto fail = [why](const QString &msg) {
        if (why) *why = msg;
        return false;
    };

    if (name.isEmpty() || name.trimmed().isEmpty())
        return fail(QStringLiteral("名称不能为空"));
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return fail(QStringLiteral("不能使用 . 或 .."));
    if (name.size() > 255)
        return fail(QStringLiteral("名称过长（最多 255 个字符）"));

    // Windows 非法字符；/ 在类 Unix 上也不允许
    static const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (const QChar c : name) {
        if (illegal.contains(c))
            return fail(QStringLiteral("名称不能包含 \\ / : * ? \" < > | 这些字符"));
        if (c.unicode() < 0x20)
            return fail(QStringLiteral("名称不能包含控制字符"));
    }

    if (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        return fail(QStringLiteral("名称不能以空格或点号结尾"));

    return true;
}

// ---------------------------------------------------------------------------
//  新建
// ---------------------------------------------------------------------------
QString newFile(QWidget *parent, const QString &dirPath)
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        parent, QStringLiteral("新建文件"),
        QStringLiteral("文件名（含扩展名）："),
        QLineEdit::Normal,
        QFileInfo(uniquePath(dirPath, QStringLiteral("新建文件.txt"))).fileName(),
        &ok);

    if (!ok)
        return QString();

    QString procName;
    if (!isValidName(name, &procName)) {
        QMessageBox::warning(parent, QStringLiteral("新建文件"), procName);
        return QString();
    }

    const QString target = QDir(dirPath).absoluteFilePath(name);
    if (QFileInfo::exists(target)) {
        QMessageBox::warning(parent, QStringLiteral("新建文件"),
                             QStringLiteral("「%1」已经存在。").arg(name));
        return QString();
    }

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(parent, QStringLiteral("新建文件"),
                             QStringLiteral("创建失败：%1").arg(file.errorString()));
        return QString();
    }
    file.close();
    return target;
}

QString newFolder(QWidget *parent, const QString &dirPath)
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        parent, QStringLiteral("新建文件夹"),
        QStringLiteral("文件夹名："),
        QLineEdit::Normal,
        QFileInfo(uniquePath(dirPath, QStringLiteral("新建文件夹"))).fileName(),
        &ok);

    if (!ok)
        return QString();

    QString procName;
    if (!isValidName(name, &procName)) {
        QMessageBox::warning(parent, QStringLiteral("新建文件夹"), procName);
        return QString();
    }

    const QString target = QDir(dirPath).absoluteFilePath(name);
    if (QFileInfo::exists(target)) {
        QMessageBox::warning(parent, QStringLiteral("新建文件夹"),
                             QStringLiteral("「%1」已经存在。").arg(name));
        return QString();
    }

    if (!QDir(dirPath).mkdir(name)) {
        QMessageBox::warning(parent, QStringLiteral("新建文件夹"),
                             QStringLiteral("创建失败，请检查是否有写权限。"));
        return QString();
    }
    return target;
}

// ---------------------------------------------------------------------------
//  重命名
// ---------------------------------------------------------------------------
QString rename(QWidget *parent, const QString &oldPath, const QString &newName,
               QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error) *error = msg;
        return QString();
    };

    const QFileInfo info(oldPath);
    if (!info.exists())
        return fail(QStringLiteral("源文件已不存在（可能被外部删除或重命名）"));

    QString procName;
    if (!isValidName(newName, &procName))
        return fail(procName);

    if (newName == info.fileName())
        return QString();                       // 没改，静默返回

    const QString target = info.absoluteDir().absoluteFilePath(newName);
    if (QFileInfo::exists(target))
        return fail(QStringLiteral("「%1」已经存在。").arg(newName));

    if (!QFile::rename(oldPath, target)) {
        // 常见原因：目标只差大小写（Windows 上同名），或文件被占用
        return fail(QStringLiteral("重命名失败，可能文件正被占用或目标同名。"));
    }

    Q_UNUSED(parent)
    return target;
}

// ---------------------------------------------------------------------------
//  删除（回收站优先）
// ---------------------------------------------------------------------------
bool removeToTrash(QWidget *parent, const QStringList &paths)
{
    if (paths.isEmpty())
        return false;

    // 一句确认：说明删的是几个、去哪
    QString detail;
    if (paths.size() == 1) {
        detail = QStringLiteral("确定要把「%1」移到回收站吗？")
                     .arg(QFileInfo(paths.first()).fileName());
    } else {
        detail = QStringLiteral("确定要把选中的 %1 项移到回收站吗？").arg(paths.size());
    }

    const auto ret = QMessageBox::question(
        parent, QStringLiteral("删除"), detail,
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ret != QMessageBox::Yes)
        return false;

    QStringList failed;
    QStringList needPermanent;

    for (const QString &p : paths) {
        const QFileInfo fi(p);
        if (!fi.exists())
            continue;

        // ⚠️ 只用 moveToTrash，不用 QFile::remove
        if (QFile::moveToTrash(p))
            continue;

        // 回收站不可用：记下来让用户二次确认「永久删除」
        needPermanent << p;
    }

    if (!needPermanent.isEmpty()) {
        const auto ret2 = QMessageBox::warning(
            parent, QStringLiteral("无法移入回收站"),
            QStringLiteral("有 %1 项无法移到回收站（可能是网络盘或权限不足）。\n\n"
                           "要**永久删除**吗？此操作不可恢复！")
                .arg(needPermanent.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (ret2 == QMessageBox::Yes) {
            for (const QString &p : needPermanent) {
                const QFileInfo fi(p);
                const bool ok = fi.isDir() ? QDir(p).removeRecursively()
                                           : QFile::remove(p);
                if (!ok)
                    failed << p;
            }
        } else {
            failed << needPermanent;
        }
    }

    if (!failed.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("删除失败"),
                             QStringLiteral("以下项目未能删除（可能被占用或权限不足）：\n%1")
                                 .arg(failed.join(QLatin1Char('\n'))));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  属性对话框
// ---------------------------------------------------------------------------
namespace {

// 人类可读大小
QString humanSize(qint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    return u == 0 ? QStringLiteral("%1 B").arg(bytes)
                  : QStringLiteral("%1 %2").arg(v, 0, 'f', 1).arg(units[u]);
}

// 递归统计目录大小（带简单深度保护，避免极深目录卡死）
qint64 dirSize(const QString &path, int depth = 0)
{
    if (depth > 24)
        return 0;
    qint64 total = 0;
    QDir d(path);
    const auto entries = d.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                             | QDir::Hidden | QDir::System);
    for (const QFileInfo &fi : entries) {
        if (fi.isDir())
            total += dirSize(fi.absoluteFilePath(), depth + 1);
        else
            total += fi.size();
    }
    return total;
}

QString permissionText(const QFileInfo &fi)
{
    const QFile::Permissions p = fi.permissions();

    // 前三位 owner、中间 user、group、other —— 用字符串拼接避免 QChar 重载歧义
    QString s;
    s += QStringLiteral("owner[%1%2] ")
             .arg((p & QFile::ReadOwner) ? QStringLiteral("r") : QStringLiteral("-"),
                  (p & QFile::WriteOwner) ? QStringLiteral("w") : QStringLiteral("-"));
    s += QStringLiteral("user[%1%2] ")
             .arg((p & QFile::ReadUser) ? QStringLiteral("r") : QStringLiteral("-"),
                  (p & QFile::WriteUser) ? QStringLiteral("w") : QStringLiteral("-"));
    s += QStringLiteral("group[%1%2] ")
             .arg((p & QFile::ReadGroup) ? QStringLiteral("r") : QStringLiteral("-"),
                  (p & QFile::WriteGroup) ? QStringLiteral("w") : QStringLiteral("-"));
    s += QStringLiteral("other[%1%2] ")
             .arg((p & QFile::ReadOther) ? QStringLiteral("r") : QStringLiteral("-"),
                  (p & QFile::WriteOther) ? QStringLiteral("w") : QStringLiteral("-"));

    return fi.isWritable() ? s + QStringLiteral("（可写）")
                           : s + QStringLiteral("（只读）");
}

QString row(const QString &label, const QString &value)
{
    return QStringLiteral("<tr><td style='color:#8a8a9d;padding:2px 10px 2px 0;'>%1</td>"
                          "<td style='color:#d4d4dc;'>%2</td></tr>")
        .arg(label, value.toHtmlEscaped());
}

} // namespace

void showProperties(QWidget *parent, const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists()) {
        QMessageBox::warning(parent, QStringLiteral("属性"),
                             QStringLiteral("文件已不存在。"));
        return;
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("属性 — %1").arg(fi.fileName()));
    dlg.setMinimumWidth(460);
    dlg.setStyleSheet(QStringLiteral("QDialog{background:#1e1e24;} "
                                     "QLabel{color:#d4d4dc;}"));

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(16, 14, 16, 12);
    lay->setSpacing(10);

    // 大标题
    auto *nameLabel = new QLabel(fi.fileName(), &dlg);
    QFont bf = nameLabel->font();
    bf.setBold(true);
    bf.setPointSize(bf.pointSize() + 2);
    nameLabel->setFont(bf);
    nameLabel->setWordWrap(true);
    lay->addWidget(nameLabel);

    // 基本信息表
    QString html = QStringLiteral("<table cellspacing='0'>");
    html += row(QStringLiteral("类型"),
                fi.isDir() ? QStringLiteral("文件夹") : QStringLiteral("文件"));
    html += row(QStringLiteral("位置"),
                QDir::toNativeSeparators(fi.absolutePath()));

    if (fi.isDir()) {
        const qint64 total = dirSize(fi.absoluteFilePath());
        const int subDirs = QDir(fi.absoluteFilePath())
                                .entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
        const int files = QDir(fi.absoluteFilePath())
                              .entryList(QDir::Files).size();
        html += row(QStringLiteral("包含"), QStringLiteral("%1 个文件，%2 个文件夹")
                                               .arg(files).arg(subDirs));
        html += row(QStringLiteral("大小"), humanSize(total) + QStringLiteral("（递归统计）"));
    } else {
        html += row(QStringLiteral("大小"),
                    QStringLiteral("%1（%2 字节）").arg(humanSize(fi.size())).arg(fi.size()));
    }
    html += row(QStringLiteral("创建时间"),
                fi.birthTime().isValid()
                    ? fi.birthTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                    : QStringLiteral("—"));
    html += row(QStringLiteral("修改时间"),
                fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    html += row(QStringLiteral("访问时间"),
                fi.lastRead().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    html += row(QStringLiteral("属性"), permissionText(fi));
    if (!fi.suffix().isEmpty())
        html += row(QStringLiteral("扩展名"), fi.suffix());
    html += QStringLiteral("</table>");

    auto *tableLabel = new QLabel(html, &dlg);
    tableLabel->setTextFormat(Qt::RichText);
    tableLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(tableLabel);

    // 完整路径（可选中复制，长路径换行）
    auto *pathBox = new QLabel(QDir::toNativeSeparators(fi.absoluteFilePath()), &dlg);
    pathBox->setWordWrap(true);
    pathBox->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathBox->setStyleSheet(QStringLiteral("color:#5a5a6a; font-size:11px;"));
    lay->addWidget(pathBox);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(box);

    dlg.exec();
}

// ---------------------------------------------------------------------------
//  在系统文件管理器中定位
// ---------------------------------------------------------------------------
void revealInExplorer(QWidget *parent, const QString &path)
{
    Q_UNUSED(parent)
#if defined(Q_OS_WIN)
    // /select,<path> 会在资源管理器里选中该项
    const QString native = QDir::toNativeSeparators(path);
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,") + native});
#else
    // 其它平台直接打开所在目录
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

} // namespace FileOps
