// ============================================================
//  UE_MusicLibrary — внешняя музыкальная библиотека мода «унесённые»
//
//  Музыку НЕ нужно паковать в PBO. Админ кладёт аудиофайлы в
//  обычную папку на сервере (по умолчанию <Profile>/Music):
//
//      Music/
//        ├─ Type/     <- кассеты:   подпапка = класс кассеты,
//        │                           внутри неё треки .ogg/.mp3/.wav
//        ├─ CD/       <- диски:      тот же принцип
//        └─ Radio.txt                список радиостанций (ключ=URL)
//
//  Как это работает:
//   * СЕРВЕР при старте сканирует эти папки (FindFileGroup),
//     строит реестр плейлистов и рассылает его всем клиентам
//     по RPC (клиентские скрипты не имеют доступа к файловой
//     системе сервера).
//   * В каждой подпапке может лежать meta.txt — метаданные:
//         name=Моя любимая кассета
//         color=255 160 40
//     Если meta.txt нет — имя = название папки.
//   * Ключ источника = относительный путь без расширения
//     (например "Type/MyMix"), он проходит серверную валидацию
//     (только реально существующие на диске плейлисты!) и
//     одинаково понятен серверу и клиенту.
//   * Клиент скачивает недостающие треки напрямую с HTTP-зеркала
//     библиотеки (параметр libraryBaseURL, пример:
//     http://<сервер>:8080/Music/) через движковый DownloadFile
//     и кэширует в <client Profile>/Music/cache.
//     Зеркало включается скриптом @KRa_TosServer/tools/serve_music.py
//     (или любой статикой: nginx, python -m http.server).
//   * Если качалка недоступна — играет штатным звуком из PBO;
//     если доступен UE_Bridge (BASS) — играет через мост.
// ============================================================

class UE_PlaylistInfo
{
    string key;         // "Type/MyMix" | "CD/MyDisc"
    string dir;         // тип: "Type" или "CD"
    string folder;      // имя папки на сервере (= часть ключа)
    string displayName; // из meta.txt или имя папки
    int trackCount;
};

class UE_RadioStationInfo
{
    string key;
    string displayName;
    string url;
    bool isDefault;   // станция зашита в config.cpp — всегда разрешена
};

class UE_MusicLibrary: ScriptModule
{
    static const string VERSION = "1.1";

    //~ --- корень библиотеки: по умолчанию <Profile>/Music ---
    static string s_RootRel;      // относительно профиля ("Music")
    static string s_RootAbs;      // абсолютный путь сервера (для логов)

    //~ --- серверные реестры ---
    static ref array<ref UE_PlaylistInfo> s_SerPlaylists;
    static ref map<string, ref UE_PlaylistInfo> s_SerIndex;   // lowercase key -> info
    static ref array<ref UE_RadioStationInfo> s_Stations;     // конфиг + Radio.txt
    static ref map<string, ref UE_RadioStationInfo> s_StIdx;  // lowercase key -> station

    //~ --- клиентский кэш ---
    static ref map<string, string> s_CachePath;               // key -> локальный путь
    static ref map<string, bool>   s_DownloadTried;           // анти-шторм повторных скачиваний
    static string s_BaseURL;                                  // HTTP-зеркало (из манифеста)

    //~ --- манифест для клиентов ---
    static string s_Manifest;
    static int s_ManifestChunkSize = 900;   // чанк под лимит пакетов RemoteExec

    void UE_MusicLibrary() {}

    //~ =========================================================
    //~  ПУТИ
    //~ =========================================================

    //~ Корень библиотеки. По умолчанию "<Profile>/Music".
    //~ Переопределяется параметром UE_Config musicRoot в config.cpp:
    //~   "~" = home-префикс движка (для клиента = My Documents\DayZ),
    //~   "~@DMS/Music" = подпапка аддонов, "/srv/music" = абсолютный путь.
    static void SetRoot(string root)
    {
        if (root.Length() == 0) root = "Music";
        // нормализуем разделители и убираем хвостовой слэш
        root = root.Replace("\\", "/");
        while (root.Length() > 1 && root.Get(root.Length() - 1) == '/')
            root = root.SubstringWithLimit(0, root.Length() - 1);
        s_RootRel = root;
        if (IsAbsolute(root))
            s_RootAbs = root;
        else
            s_RootAbs = GetGame().GetProfileDir() + root;   // профиль уже заканчивается '/'
    }

    static bool IsAbsolute(string p)
    {
        if (p.Length() == 0) return false;
        if (p.Get(0) == '~') return true;                  // движковый home-префикс (~, ~@Addon, ~/...)
        if (p.Get(0) == '/') return true;                  // unix-путь /srv/music
        if (p.Length() >= 2)
        {
            char c0 = p.Get(0);
            bool letter = (c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z');
            if (letter && p.Get(1) == ':') return true;    // Windows: C:/music
        }
        return false;
    }

    static string Full(string rel)
    {
        if (IsAbsolute(s_RootRel)) return s_RootRel + "/" + rel;
        return GetGame().GetProfileDir() + s_RootRel + "/" + rel;
    }

    //~ Локальный кэш клиента: <Profile>/Music_cache
    static string CacheDir()
    {
        return GetGame().GetProfileDir() + "Music_cache/";
    }

    //~ =========================================================
    //~  СЕРВЕР: сканирование папок Music/Type и Music/CD
    //~ =========================================================

    static void ServerScan()
    {
        InitDefaults();

        // гарантируем структуру, чтобы админу было куда класть файлы
        MakeDirectory(s_RootAbs);
        MakeDirectory(Full("Type"));
        MakeDirectory(Full("CD"));

        ScanMediaDir("Type");
        ScanMediaDir("CD");

        Print("[UE_MusicLibrary] сервер: найдено плейлистов = " + s_SerPlaylists.Count() +
              ", корень = " + s_RootAbs);
    }

    static void InitDefaults()
    {
        if (!s_SerPlaylists) s_SerPlaylists = new array<ref UE_PlaylistInfo>;
        if (!s_SerIndex)     s_SerIndex     = new map<string, ref UE_PlaylistInfo>;
        if (!s_Stations)     s_Stations     = new array<ref UE_RadioStationInfo>;
        if (!s_StIdx)        s_StIdx        = new map<string, ref UE_RadioStationInfo>;

        // станции из config.cpp (белый список) — помечаем как default
        if (s_StIdx.Empty())
        {
            AddStationInternal("Apex",       "Апекс ФМ",     "http://62.152.59.3:8000/nkz",                 true);
            AddStationInternal("EuropaPlus", "Европа Плюс",  "http://online-2.gkvr.ru:8000/europa_nkz_64.aac", true);
            AddStationInternal("HumorFM",    "Юмор FM",      "http://62.231.184.253:8000/humor",             true);
        }
    }

    static void MakeDirectory(string path)
    {
        if (!FileExists(path))
        {
            // CreateFolder(<путь>, <рекурсивно>) — движковая фабрика файлов.
            // Путь не должен заканчиваться слэшем.
            string p = path;
            while (p.Length() > 1 && (p.Get(p.Length()-1) == '/' || p.Get(p.Length()-1) == '\\'))
                p = p.SubstringWithLimit(0, p.Length() - 1);
            GetFileFactory(GetFileFactoryType()).CreateFolder(p, true);
        }
    }

    static void ScanMediaDir(string dirName)   // "Type" или "CD"
    {
        string rootFull = Full(dirName);                 // .../Music/Type
        array<string> groups = {};
        // ищем все подпапки первого уровня внутри Music/Type и Music/CD
        FindFileGroup(rootFull + "/*", groups, false);

        for (int i = 0; i < groups.Count(); i++)
        {
            string gpath = groups.Get(i);
            // нормализуем разделители, убираем хвостовые слэши
            while (gpath.Contains("\\\")) gpath = gpath.Replace("\\\", "/");
            while (gpath.Length() > 0 && gpath.Get(gpath.Length()-1) == '/')
                gpath = gpath.SubstringWithLimit(0, gpath.Length() - 1);

            // имя папки = последний сегмент после rootFull/
            string rel = gpath;
            int cut = gpath.IndexOf(rootFull);
            if (cut == 0) rel = gpath.SubstringWithLimit(rootFull.Length() + 1, gpath.Length() - rootFull.Length() - 1);
            while (rel.Contains("/")) rel = rel.Replace("/", "_");   // защита от вложенности
            if (rel.Length() == 0) continue;

            // уже зарегистрированы?
            string key = dirName + "/" + rel;
            UE_PlaylistInfo dummy;
            if (s_SerIndex.Find(key.ToLower(), dummy)) continue;

            // есть ли медиафайлы в этой папке?
            int n = CountMedia(gpath);
            if (n == 0) continue;

            UE_PlaylistInfo info = new UE_PlaylistInfo;
            info.key = key;
            info.dir = dirName;
            info.folder = rel;
            info.displayName = ReadMetaName(gpath, rel);
            info.trackCount = n;

            s_SerPlaylists.Insert(info);
            s_SerIndex.Set(key.ToLower(), info);
            Print("[UE_MusicLibrary] плейлист: " + key + " (" + info.displayName + ", треков: " + n + ")");
        }
    }

    static int CountMedia(string folderPath)
    {
        string fp = folderPath;
        while (fp.Length() > 0 && (fp.Get(fp.Length()-1) == '/' || fp.Get(fp.Length()-1) == '\\'))
            fp = fp.SubstringWithLimit(0, fp.Length() - 1);
        array<string> files = {};
        int total = 0;
        if (FindFile(fp, "*.ogg", files, false)) total += files.Count();
        files.Clear();
        if (FindFile(folderPath, "*.mp3", files, false)) total += files.Count();
        files.Clear();
        if (FindFile(folderPath, "*.wav", files, false)) total += files.Count();
        return total;
    }

    static string ReadMetaName(string folderPath, string fallback)
    {
        string metaPath = folderPath;
        while (metaPath.Length() > 0 && (metaPath.Get(metaPath.Length()-1) == '/' || metaPath.Get(metaPath.Length()-1) == '\\'))
            metaPath = metaPath.SubstringWithLimit(0, metaPath.Length() - 1);
        metaPath = metaPath + "/meta.txt";
        if (!FileExists(metaPath)) return fallback;
        FileHandle f = OpenFile(metaPath, FileMode.READ);
        if (FileIsSame(f, NULL)) return fallback;
        string line;
        string name = "";
        while (FGets(f, line) >= 0)
        {
            line = Trim(line);
            if (line.Length() > 5 && line.SubstringWithLimit(0, 5) == "name=")
            {
                name = Trim(line.SubstringWithLimit(5, line.Length() - 5));
                break;
            }
        }
        CloseFile(f);
        if (name.Length() == 0) return fallback;
        return name;
    }

    static string Trim(string s)
    {
        int a = 0, b = s.Length();
        while (a < b && (s.Get(a) == ' ' || s.Get(a) == '\t' || s.Get(a) == '\r' || s.Get(a) == '\n')) a++;
        while (b > a && (s.Get(b - 1) == ' ' || s.Get(b - 1) == '\t' || s.Get(b - 1) == '\r' || s.Get(b - 1) == '\n')) b--;
        return s.SubstringWithLimit(a, b - a);
    }

    //~ =========================================================
    //~  СЕРВЕР: радиостанции из Music/Radio.txt
    //~  Формат (одна станция на строку):  Название = URL
    //~  Строки "#..." — комментарии. URL проверяются на http(s)://
    //~ =========================================================

    static void ServerLoadRadioTxt()
    {
        InitDefaults();
        string path = Full("Radio.txt");
        if (!FileExists(path))
        {
            CreateDefaultRadioTxt(path);
        }
        FileHandle f = OpenFile(path, FileMode.READ);
        if (FileIsSame(f, NULL)) { Print("[UE_MusicLibrary] Radio.txt: не удалось открыть"); return; }

        string line;
        int added = 0;
        while (FGets(f, line) >= 0)
        {
            line = Trim(line);
            if (line.Length() == 0) continue;
            if (line.Get(0) == '#') continue;
            int eq = line.IndexOf("=");
            if (eq <= 0) continue;
            string name = Trim(line.SubstringWithLimit(0, eq));
            string url  = Trim(line.SubstringWithLimit(eq + 1, line.Length() - eq - 1));
            if (url.Length() == 0) continue;
            if (!(url.StartsWith("http://") || url.StartsWith("https://")))
            {
                Print("[UE_MusicLibrary] Radio.txt: пропущена не-HTTP ссылка: " + url);
                continue;
            }
            string key = SanitizeKey(name);
            UE_RadioStationInfo dummy;
            if (s_StIdx.Find(key.ToLower(), dummy)) continue;
            AddStationInternal(key, name, url, false);
            added++;
        }
        CloseFile(f);
        Print("[UE_MusicLibrary] Radio.txt: добавлено станций = " + added);
    }

    static void CreateDefaultRadioTxt(string path)
    {
        FileHandle f = OpenFile(path, FileMode.WRITE);
        if (FileIsSame(f, NULL)) return;
        FPutS(f, "# унесённые — список радиостанций\n");
        FPutS(f, "# формат: Название = URL потока (http/https)\n");
        FPutS(f, "# Апекс, Европа+ и Юмор FM зашиты в конфиг и доступны всегда.\n");
        FPutS(f, "# EuropaPlus = http://online-2.gkvr.ru:8000/europa_nkz_64.aac\n");
        CloseFile(f);
        Print("[UE_MusicLibrary] создан шаблон " + path);
    }

    static void AddStationInternal(string key, string disp, string url, bool def)
    {
        UE_RadioStationInfo st = new UE_RadioStationInfo;
        st.key = key; st.displayName = disp; st.url = url; st.isDefault = def;
        s_Stations.Insert(st);
        s_StIdx.Set(key.ToLower(), st);
    }

    static string SanitizeKey(string name)
    {
        string k = "";
        for (int i = 0; i < name.Length(); i++)
        {
            char c = name.Get(i);
            bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            k.Append(ok ? c.AsString() : "_");
        }
        if (k.Length() == 0) k = "station";
        return k;
    }

    //~ =========================================================
    //~  ВАЛИДАЦИЯ (используется UE_Security + сетевым слоем)
    //~ =========================================================

    static bool HasPlaylist(string key)
    {
        if (!s_SerIndex) return false;
        UE_PlaylistInfo dummy;
        return s_SerIndex.Find(key.ToLower(), dummy);
    }

    static bool HasStation(string key)
    {
        if (!s_StIdx) return false;
        UE_RadioStationInfo dummy;
        return s_StIdx.Find(key.ToLower(), dummy);
    }

    static string GetStationUrlByKey(string key)
    {
        UE_RadioStationInfo st;
        if (s_StIdx && s_StIdx.Find(key.ToLower(), st)) return st.url;
        return "";
    }

    static string GetPlaylistDisplay(string key)
    {
        UE_PlaylistInfo p;
        if (s_SerIndex && s_SerIndex.Find(key.ToLower(), p)) return p.displayName;
        return key;
    }

    static string GetStationNameSafe(string key)
    {
        UE_RadioStationInfo st;
        if (s_StIdx && s_StIdx.Find(key.ToLower(), st)) return st.displayName;
        return "";
    }

    //~ =========================================================
    //~  МАНИФЕСТ ДЛЯ КЛИЕНТОВ
    //~  Формат: base64( "ver|baseUrl|nPlays|key;disp|...|nSt|key;disp;url64|" )
    //~  (url кодируется отдельно, т.к. содержит '=' и ':')
    //~ =========================================================

    static string EncodeManifest(string baseUrl)
    {
        StringBuilder sb = new StringBuilder;
        sb.Append(VERSION);
        sb.Append("|");
        sb.Append(baseUrl);
        sb.Append("|");
        sb.Append(s_SerPlaylists.Count());
        for (int i = 0; i < s_SerPlaylists.Count(); i++)
        {
            UE_PlaylistInfo p = s_SerPlaylists.Get(i);
            sb.Append("|");
            sb.Append(p.key);
            sb.Append(";");
            sb.Append(p.displayName.Replace(";", ","));
        }
        sb.Append("|");
        sb.Append(s_Stations.Count());
        for (int j = 0; j < s_Stations.Count(); j++)
        {
            UE_RadioStationInfo st = s_Stations.Get(j);
            sb.Append("|");
            sb.Append(st.key);
            sb.Append(";");
            sb.Append(st.displayName.Replace(";", ","));
            sb.Append(";");
            sb.Append(EncodeBase64(st.url));
        }
        return EncodeBase64(sb.ToString());
    }

    static string DecodeBase64(string enc)
    {
        if (enc.Length() == 0) return "";
        array<ref> bytes = {};
        if (!Codec.DecodeBase64(enc, bytes)) return "";
        string outStr = "";
        for (int i = 0; i < bytes.Count(); i++)
        {
            BinItem bi = BinItem.Cast(bytes.Get(i));
            if (!bi) continue;
            for (int b = 0; b < bi.Count(); b++)
            {
                char c = char.Parse(bi.Get(b));
                outStr.Append("" + c);
            }
        }
        return outStr;
    }

    static string EncodeBase64(string src)
    {
        array<ref> bytes = {};
        BinItem bi = new BinItem;
        for (int i = 0; i < src.Length(); i++)
            bi.Insert(ToInt(src.Get(i)));
        bytes.Insert(bi);
        string enc;
        if (!Codec.EncodeBase64(bytes, enc)) return "";
        return enc;
    }

    static int ManifestChunks()
    {
        if (s_Manifest.Length() == 0) return 0;
        return (s_Manifest.Length() + s_ManifestChunkSize - 1) / s_ManifestChunkSize;
    }

    static string ManifestChunk(int idx)
    {
        int off = idx * s_ManifestChunkSize;
        if (off >= s_Manifest.Length()) return "";
        return s_Manifest.SubstringWithLimit(off, s_ManifestChunkSize);
    }

    //~ =========================================================
    //~  КЛИЕНТ: приём манифеста
    //~ =========================================================

    static void ClientOnManifestBegin(string manifestB64)
    {
        s_CachePath = null; s_DownloadTried = null;   // библиотека изменилась — сброс кэша
        s_Manifest = manifestB64;
    }

    static void ClientOnManifestEnd()
    {
        string raw = DecodeBase64(s_Manifest);
        array<string> parts = {};
        raw.Split("|", parts);
        if (parts.Count() < 3) { Print("[UE_MusicLibrary] манифест пуст/битый"); return; }

        s_BaseURL = parts.Get(1);

        // пересобираем реестры на клиенте (нужны для отображения имён)
        s_SerPlaylists = new array<ref UE_PlaylistInfo>;
        s_SerIndex     = new map<string, ref UE_PlaylistInfo>;
        s_Stations     = new array<ref UE_RadioStationInfo>;
        s_StIdx        = new map<string, ref UE_RadioStationInfo>;

        int idx = 2;
        int nPlays = 0;
        if (idx < parts.Count()) { nPlays = parts.Get(idx).ToInt(); idx++; }
        for (int i = 0; i < nPlays && idx + 0 < parts.Count(); i++)
        {
            array<string> kv = {}; parts.Get(idx).Split(";", kv); idx++;
            if (kv.Count() < 2) continue;
            UE_PlaylistInfo p = new UE_PlaylistInfo;
            p.key = kv.Get(0);
            p.displayName = kv.Get(1);
            p.dir = p.key.SubstringWithLimit(0, p.key.IndexOf("/"));
            p.folder = p.key.SubstringWithLimit(p.key.IndexOf("/") + 1, p.key.Length());
            s_SerPlaylists.Insert(p);
            s_SerIndex.Set(p.key.ToLower(), p);
        }
        int nSt = 0;
        if (idx < parts.Count()) { nSt = parts.Get(idx).ToInt(); idx++; }
        for (int j = 0; j < nSt && idx < parts.Count(); j++)
        {
            array<string> kv = {}; parts.Get(idx).Split(";", kv); idx++;
            if (kv.Count() < 3) continue;
            UE_RadioStationInfo st = new UE_RadioStationInfo;
            st.key = kv.Get(0);
            st.displayName = kv.Get(1);
            st.url = DecodeBase64(kv.Get(2));
            st.isDefault = (st.url.Length() > 0);
            s_Stations.Insert(st);
            s_StIdx.Set(st.key.ToLower(), st);
        }

        s_CachePath = new map<string, string>;
        s_DownloadTried = new map<string, bool>;
        Print("[UE_MusicLibrary] клиент: библиотека получена — плейлистов: " + nPlays +
              ", станций: " + nSt + ", зеркало: " + s_BaseURL);
    }

    static bool ClientHasPlaylist(string key)
    {
        if (!s_SerIndex) return false;
        UE_PlaylistInfo dummy;
        return s_SerIndex.Find(key.ToLower(), dummy);
    }

    static bool ClientHasStation(string key)
    {
        if (!s_StIdx) return false;
        UE_RadioStationInfo dummy;
        return s_StIdx.Find(key.ToLower(), dummy);
    }

    static string ClientStationUrl(string key)
    {
        UE_RadioStationInfo st;
        if (s_StIdx && s_StIdx.Find(key.ToLower(), st)) return st.url;
        return "";
    }

    static string ClientDisplayName(string key)
    {
        UE_PlaylistInfo p;
        if (s_SerIndex && s_SerIndex.Find(key.ToLower(), p)) return p.displayName;
        return key;
    }

    //~ =========================================================
    //~  КЛИЕНТ: разрешение ключа в играбельный файл.
    //~  1) локальный кэш  2) докачка с HTTP-зеркала (асинхронно,
    //~     следующий запрос увидит файл)  3) null — пусть играет
    //~     штатный слой из PBO.
    //~ =========================================================

    static string ResolveLocalFile(string key)
    {
        if (key.Length() == 0) return null;
        // защита от path traversal — только [A-Za-z0-9_/]
        for (int i = 0; i < key.Length(); i++)
        {
            char c = key.Get(i);
            bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '_' || c == '/' || c == '-';
            if (!ok) return null;
        }

        string exts[3]; exts[0] = ".ogg"; exts[1] = ".mp3"; exts[2] = ".wav";
        for (int e = 0; e < 3; e++)
        {
            string local = CacheDir() + key + exts[e];
            if (FileExists(local)) return local;
        }

        TryDownload(key);
        return null;
    }

    static void TryDownload(string key)
    {
        // синхронная версия — НЕ используется из игрового тика (блокирует поток).
        // оставлена как утилита для оффлайн-скриптов.
        TryDownloadAsync(key);
    }

    static bool HasCachedFile(string key)
    {
        string exts[3]; exts[0] = ".ogg"; exts[1] = ".mp3"; exts[2] = ".wav";
        for (int e = 0; e < 3; e++)
        {
            if (FileExists(CacheDir() + key + exts[e])) return true;
        }
        return false;
    }

    //~ Асинхронная докачка через движковый AsyncFileDownloader
    //~ (событие OnDownloadFinished обрабатывает UE_DownloadWatcher).
    static void TryDownloadAsync(string key)
    {
        if (!s_DownloadTried) return;
        if (s_DownloadTried.GetOrAdd(key, false)) return;   // уже пробовали
        s_DownloadTried.Set(key, true);

        if (!s_BaseURL || s_BaseURL.Length() == 0) return;

        string url = s_BaseURL;
        if (url.Get(url.Length() - 1) != '/') url.Append("/");
        url.Append(key);

        string local = CacheDir() + key;
        MakeDirectoryRecursive(CacheDir() + key.SubstringWithLimit(0, key.IndexOf("/") + 1));

        // скачиваем только тот формат, которого ещё нет локально
        ref array<string> urls = new array<string>;
        ref array<string> dests = new array<string>;
        string exts[3]; exts[0] = ".ogg"; exts[1] = ".mp3"; exts[2] = ".wav";
        for (int e = 0; e < 3; e++)
        {
            if (!FileExists(local + exts[e]))
            {
                urls.Insert(url + exts[e]);
                dests.Insert(local + exts[e]);
            }
        }
        if (urls.Count() == 0) return;

        ref ScriptParam_ArrayString urlsp = new ScriptParam_ArrayString;
        urlsp.Set(urls);
        ref ScriptParam_ArrayString destsp = new ScriptParam_ArrayString;
        destsp.Set(dests);

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(
            GetGame().CreateAsyncFileDownloader(), "Download", urlsp, destsp,
            "OnDownloadFinished", "OnDownloadProgress", CALL_STATE_OK);
        Print("[UE_MusicLibrary] докачка поставлена в очередь: " + url);
    }

    static void MakeDirectoryRecursive(string path)
    {
        if (path.Length() == 0) return;
        if (!FileExists(path))
            GetFileFactory(GetFileFactoryType()).CreateFolder(path, true);
    }
};
