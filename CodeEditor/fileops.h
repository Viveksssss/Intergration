#pragma once

#include <QString>

class QWidget;

// ---------------------------------------------------------------------------
//  FileOps —— 资源管理器用到的文件操作，全部做成「静态函数」
//
//  为什么不直接写在 FileExplorer 里？
//    1. 这些操作和界面无关，独立出来方便单元测试 / 复用（比如以后加到菜单栏）；
//    2. FileExplorer 只负责「弹菜单」，操作细节不该混进视图代码。
//
//  ⚠️ 安全约定（重要）
//    - 删除一律走「回收站」(QFile::moveToTrash)，**绝不直接 unlink**。
//      回收站不可用（网络盘/某些 Linux 挂载点）时才弹二次确认走永久删除。
//    - 所有会覆盖 / 会丢数据的操作，都先用 QMessageBox 确认。
// ---------------------------------------------------------------------------
namespace FileOps {

// 在当前目录下生成一个不冲突的名字：新建文件.txt / 新建文件 2.txt …
QString uniquePath(const QString &dirPath, const QString &baseName,
                   const QString &suffix = QString());

// 新建空文件。成功返回新文件绝对路径，失败返回空串
QString newFile(QWidget *parent, const QString &dirPath);

// 新建子文件夹。成功返回新目录绝对路径
QString newFolder(QWidget *parent, const QString &dirPath);

// 重命名：oldPath -> 同目录下的 newName。
// 返回新路径；用户取消 / 失败则返回空串，失败原因用 error 带回。
QString rename(QWidget *parent, const QString &oldPath, const QString &newName,
               QString *error = nullptr);

// 删除到回收站。返回 true 表示已删（或用户确认删除成功）
bool removeToTrash(QWidget *parent, const QStringList &paths);

// 属性对话框（模态）
void showProperties(QWidget *parent, const QString &path);

// 在系统文件管理器中定位（explorer /select,）
void revealInExplorer(QWidget *parent, const QString &path);

// 名字是否合法（不含 \ / : * ? " < > |，且不是空、不是 . 和 ..）
bool isValidName(const QString &name, QString *why = nullptr);

// 把「文件名带扩展名」拆开，用于重命名时只选中主干部分
int    baseNameLength(const QString &fileName);

} // namespace FileOps
