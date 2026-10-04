// ============================================================
//  UEOverlay.cs — внешний аудио-оверлей мода «унесённые»
//  Компаньон клиента DayZ: читает очередь команд commands.q,
//  вызывает UEAudioBridge.dll (BASS), пишет bridge_status.txt.
//
//  Сборка (.NET Framework 4.8 / x64):
//    csc /platform:x64 /out:UEOverlay.exe UEOverlay.cs
//  Запуск: UEOverlay.exe "D:\Steam\steamapps\common\DayZ"
//  (путь = корень клиента, там же папка unesennye_bridge\)
//
//  ВАЖНО: подпись команд (FNV-1a) должна совпадать с ключом из
//  UE_SoundBackend.c (m_Key). Команды с неверной подписью игнорируются.
// ============================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

static class NativeBridge
{
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_Initialize(float masterDb, float gain);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void UE_Shutdown();
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
    public static extern bool UE_PlayFile(string path, bool loop, float vol, float x, float y, float z, out uint handle);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
    public static extern bool UE_PlayStream(string url, float vol, float x, float y, float z, uint bufMs, out uint handle);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_Stop(uint handle);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_SetVolume(uint handle, float v);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_SetPosition(uint handle, float x, float y, float z);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_SetListenerPosition(float x, float y, float z);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_GetState(uint handle, out bool playing, out float posSec);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern bool UE_IsStreamConnected(uint handle, out bool connected);
    [DllImport("UEAudioBridge.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
    public static extern string UE_GetLastError();
}

class UEOverlay
{
    const string KEY = "UE-BRIDGE-KEY-2026"; // тот же, что в UE_SoundBackend.c

    static uint Fnv1a(string s)
    {
        uint h = 2166136261;
        foreach (byte b in Encoding.UTF8.GetBytes(s)) { h ^= b; h *= 16777619; }
        return h;
    }

    static int Main(string[] args)
    {
        string root = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();
        string dir = Path.Combine(root, "unesennye_bridge");
        string qPath = Path.Combine(dir, "commands.q");
        string stPath = Path.Combine(dir, "bridge_status.txt");
        long offset = 0;

        if (!NativeBridge.UE_Initialize(-6.0f, 1.0f))
        {
            Console.WriteLine("[UEOverlay] init fail: " + NativeBridge.UE_GetLastError());
            return 1;
        }
        Console.WriteLine("[UEOverlay] мост запущен, очередь: " + qPath);

        var sources = new Dictionary<int, uint>(); // srcId -> BASS handle

        while (true)
        {
            // ---- читаем новые команды ----
            if (File.Exists(qPath))
            {
                using (var fs = new FileStream(qPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    if (fs.Length < offset) offset = 0; // файл был ротирован
                    fs.Seek(offset, SeekOrigin.Begin);
                    var br = new StreamReader(fs, Encoding.UTF8);
                    string line;
                    while ((line = br.ReadLine()) != null)
                    {
                        ProcessLine(line, sources);
                    }
                    offset = fs.Position;
                }
            }

            // ---- пишем статус ----
            var sb = new StringBuilder();
            foreach (var kv in sources)
            {
                bool playing, conn; float pos;
                NativeBridge.UE_GetState(kv.Value, out playing, out pos);
                NativeBridge.UE_IsStreamConnected(kv.Value, out conn);
                sb.AppendLine(kv.Key + "|" + (playing ? 1 : 0) + "|" + pos.ToString("F1") + "|" + (conn ? 1 : 0));
            }
            File.WriteAllText(stPath, sb.ToString());

            Thread.Sleep(500);
        }
    }

    static void ProcessLine(string line, Dictionary<int, uint> sources)
    {
        int p = line.LastIndexOf('|');
        if (p <= 0) return;
        string body = line.Substring(0, p);
        string sig = line.Substring(p + 1);
        if (sig != Fnv1a(body + "|" + KEY).ToString()) return; // защита от подброса

        string[] f = body.Split('|');
        if (f.Length < 8) return;
        int op = int.Parse(f[0]);
        int id = int.Parse(f[1]);
        string arg = f[2];
        float a = float.Parse(f[3]), x = float.Parse(f[4]), y = float.Parse(f[5]), z = float.Parse(f[6]);
        bool loop = f[7] == "1";
        uint h;

        switch (op)
        {
            case 1: // PlayFile
                if (NativeBridge.UE_PlayFile(arg, loop, a, x, y, z, out h)) sources[id] = h;
                else Console.WriteLine("[UEOverlay] file fail: " + NativeBridge.UE_GetLastError());
                break;
            case 2: // PlayStream
                if (NativeBridge.UE_PlayStream(arg, a, x, y, z, 5000, out h)) sources[id] = h;
                else Console.WriteLine("[UEOverlay] stream fail: " + NativeBridge.UE_GetLastError());
                break;
            case 3: // Stop
                if (sources.TryGetValue(id, out h)) { NativeBridge.UE_Stop(h); sources.Remove(id); }
                break;
            case 4: // SetVolume
                if (sources.TryGetValue(id, out h)) NativeBridge.UE_SetVolume(h, a);
                break;
            case 5: // SetPosition
                if (sources.TryGetValue(id, out h)) NativeBridge.UE_SetPosition(h, x, y, z);
                break;
            case 6: // SetListener
                NativeBridge.UE_SetListenerPosition(x, y, z);
                break;
        }
    }
}
