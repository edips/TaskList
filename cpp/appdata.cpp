#include "appdata.h"

#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

#ifdef Q_OS_ANDROID
#include <QAndroidJniObject>
#include <QtAndroid>
#endif

#include "system.h"

AppData::AppData(QObject *parent)
    : QObject(parent)
    , m_currentList(nullptr)
{
    m_lists = new QQmlObjectListModel<List>(this);

    m_listFilePath = System::dataRoot() + "/lists.json";
    qDebug() << m_listFilePath;

    if (!checkDirs())
        qFatal("App won't work - cannot create data directory.");
    readListFile();
}

AppData::~AppData()
{
    writeListFile();
}

AppData &AppData::instance()
{
    static AppData instance;

    return instance;
}

bool AppData::checkDirs() const
{
    QDir myDir;
    auto path = System::dataRoot();

    if (!myDir.exists(path)) {
        if (!myDir.mkpath(path)) {
            qWarning() << "Cannot create" << path;
            return false;
        }
        qDebug() << "Created directory" << path;
    }

    return true;
}

void AppData::readListFile()
{
    qDebug() << "Read the list database";

    QFile readFile(m_listFilePath);

    if (!readFile.exists()) {
        qWarning() << "List cache doesn't exist:" << m_listFilePath;
        return;
//        qDebug() << "Creating list file from cache";
//        QFile::copy(":/assets/list.json", m_listFilePath);
//        QFile f(":/data-assets/lists.json");
//        if (!f.copy(m_listFilePath)) {
//            qDebug() << f.errorString();
//            return;
//        }
//        readFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    if (!readFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << m_listFilePath;
        return;
    }
    auto jdoc = QJsonDocument::fromJson(readFile.readAll());
    readFile.close();
    if (!jdoc.isObject()) {
        qWarning() << "Cannot read JSON file:" << m_listFilePath;
        return;
    }
    auto jobj = jdoc.object();
    for (const auto o : jobj["lists"].toArray())
        m_lists->append(List::fromJson(o.toObject()));
    auto curList = jobj["current"];
    if (!curList.isNull() && !curList.toString().isEmpty())
        selectList(curList.toString());

    qDebug() << m_lists->count() << "lists loaded";
    qDebug() << "List database read";
}

void AppData::writeListFile() const
{
    qDebug() << "Write the list file";

    QFile writeFile(m_listFilePath);

    if (!writeFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open file:" << m_listFilePath;
        return;
    }
    QJsonObject jobj;
    QJsonArray jarrLists;
    for (const auto &i : m_lists->toList())
        jarrLists.append(i->toJson());
    jobj["lists"] = jarrLists;
    jobj["current"] = currentList() ? currentList()->name() : "";
    writeFile.write(QJsonDocument(jobj).toJson());
    writeFile.close();

    qDebug() << "List saved";
}

List *AppData::findList(const QString &name) const
{
    for (const auto &l : m_lists->toList()) {
        if (l->name() == name)
            return l;
    }
    return nullptr;
}

bool AppData::addList(const QString &name) const
{
    if (findList(name))
        return false;
    m_lists->append(new List(name));

    return true;
}

void AppData::selectList(const QString &name)
{
    auto list = findList(name);
    if (list)
        setCurrentList(list);
}

void AppData::removeList(const QString &name)
{
    auto list = findList(name);
    if (list) {
        auto idx = m_lists->indexOf(list);
        removeList(idx);
    }
}

void AppData::removeList(int index)
{
    if (index >= 0 && index < m_lists->count()) {
        m_lists->remove(index);
        if (index > 0 && index >= m_lists->count())
            index--;
        if (m_lists->count())
            setCurrentList(m_lists->at(index));
        else
            setCurrentList(nullptr);
    }
}

#ifdef Q_OS_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_github_stemoretti_tasklist_MainActivity_sendResult(JNIEnv *env,
                                                            jobject obj,
                                                            jstring text)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    auto result = QAndroidJniObject(text).toString();
    if (!result.isEmpty())
        emit AppData::instance().speechRecognized(result);
}

void AppData::startSpeechRecognizer() const
{
    QtAndroid::androidActivity().callMethod<void>("getSpeechInput", "()V");
}

void AppData::sendNotification(const QString &s) const
{
    auto javaNotification = QAndroidJniObject::fromString(s);
    QtAndroid::androidActivity().callMethod<void>("notify", "(Ljava/lang/String;)V",
                                                  javaNotification.object<jstring>());
}

void AppData::setAlarm() const
{
    QtAndroid::androidActivity().callMethod<void>("setAlarm", "()V");
}

void AppData::cancelAlarm() const
{
    QtAndroid::androidActivity().callMethod<void>("cancelAlarm", "()V");
}
#endif

//{{{ Properties getters/setters definitions

List *AppData::currentList() const
{
    return m_currentList;
}

void AppData::setCurrentList(List *currentList)
{
    if (m_currentList == currentList)
        return;

    m_currentList = currentList;
    emit currentListChanged(m_currentList);
}

//}}} Properties getters/setters definitions
