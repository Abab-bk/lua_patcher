// protectgen — EverythingRandomizer protection dataset generator.
//
// Reads the Skyrim plugin files (.esm/.esp/.esl) OUTSIDE the game process and
// writes examples/EverythingRandomizer/EverythingRandomizer_protection.lua:
// quest alias references (typed + raw subrecord scan) and default-object
// (DOBJ) references, protected against shuffling by the randomizer.
//
// Run with no arguments for interactive mode (prompts for the Skyrim
// directory), or headless:
//
//   protectgen <SkyrimDataOrGameFolder> [--ccc <Skyrim.ccc>]
//              [--plugins <plugins.txt>] [--mods-dir <MO2 mods folder>]
//              [--out <file>] [--json <file>]
//
// --plugins includes a modded load order (plugins.txt, MO2 format); files are
// resolved from the Data directory or the MO2 mods staging folders (later
// overrides earlier, files at mod root or under Data/).
// --ccc defaults to auto-detection (Data/Skyrim.ccc or game root/Skyrim.ccc).
// --out defaults to EverythingRandomizer_protection.lua in the working dir.
// --json additionally dumps the raw dataset (questAliasRefs, dobjRefs,
// formInfo) for debugging.
//
// Record types are reported by their Mutagen getter-interface names.
// License: GPL-3.0.

using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;
using Mutagen.Bethesda.Skyrim;

Console.OutputEncoding = Encoding.UTF8;

// ---- argument parsing ----
var dataDir = args.Length < 1 ? "" : args[0];
var cccPath = "";
var pluginsPath = "";
var modsDir = "";
var outPath = "";
var jsonPath = "";
for (var i = 1; i < args.Length; i++)
{
    if (args[i] == "--ccc" && i + 1 < args.Length) cccPath = args[++i];
    if (args[i] == "--plugins" && i + 1 < args.Length) pluginsPath = args[++i];
    if (args[i] == "--mods-dir" && i + 1 < args.Length) modsDir = args[++i];
    if (args[i] == "--out" && i + 1 < args.Length) outPath = args[++i];
    if (args[i] == "--json" && i + 1 < args.Length) jsonPath = args[++i];
}

// ---- interactive mode ----
if (dataDir.Length == 0)
{
    Console.WriteLine("protectgen: EverythingRandomizer protection dataset generator");
    Console.WriteLine();

    // Native folder/file picker on Windows; cancel or a non-Windows run
    // falls back to typing the path.
    var picked = PickPath("Select the Skyrim folder (game root or Data dir)", includeFiles: false);
    if (picked != null)
    {
        dataDir = picked;
    }
    else
    {
        Console.Write("Skyrim folder (game root or Data dir): ");
        var input = Console.ReadLine()?.Trim().Trim('"') ?? "";
        if (input.Length == 0)
        {
            Console.WriteLine("No input, exiting.");
            return 1;
        }
        dataDir = input;
    }

    var pickedPlugin = PickPath("Select plugins.txt (cancel for vanilla + Creation Club only)", includeFiles: true);
    if (pickedPlugin != null)
    {
        pluginsPath = pickedPlugin;
    }
    else
    {
        Console.Write("plugins.txt path (Enter for vanilla + Creation Club only): ");
        pluginsPath = (Console.ReadLine()?.Trim().Trim('"') ?? "").Trim();
    }
    if (pluginsPath.Length > 0)
    {
        var pickedMods = PickPath("Select the MO2 mods staging folder (cancel to skip)", includeFiles: false);
        if (pickedMods != null)
        {
            modsDir = pickedMods;
        }
        else
        {
            Console.Write("MO2 mods folder (Enter to skip, plugins must be in Data): ");
            modsDir = (Console.ReadLine()?.Trim().Trim('"') ?? "").Trim();
        }
    }
    Console.WriteLine();
}

// Native Windows shell picker (SHBrowseForFolder); null when unavailable,
// cancelled, or the path cannot be resolved. The dialog also lets the user
// pick files (plugins.txt) when includeFiles is set.
static string? PickPath(string title, bool includeFiles)
{
    if (!OperatingSystem.IsWindows())
    {
        return null;
    }
    try
    {
        var info = new NativeDialogs.BROWSEINFO
        {
            lpszTitle = title,
            ulFlags = NativeDialogs.BIF_NEWDIALOGSTYLE |
                (includeFiles ? NativeDialogs.BIF_BROWSEINCLUDEFILES : NativeDialogs.BIF_RETURNONLYFSDIRS),
        };
        var pidl = NativeDialogs.SHBrowseForFolder(ref info);
        if (pidl == IntPtr.Zero)
        {
            return null;  // cancelled
        }
        try
        {
            var path = new StringBuilder(1024);
            if (!NativeDialogs.SHGetPathFromIDList(pidl, path))
            {
                return null;
            }
            return path.Length > 0 ? path.ToString() : null;
        }
        finally
        {
            Marshal.FreeCoTaskMem(pidl);
        }
    }
    catch (Exception)
    {
        return null;  // shell32 missing (non-Windows) or dialog failure
    }
}

// ---- paths ----
static string? ResolveDataDir(string input)
{
    var p = Path.GetFullPath(input);
    if (!Directory.Exists(p)) return null;
    if (Directory.Exists(Path.Combine(p, "Data"))) return Path.Combine(p, "Data");
    if (File.Exists(Path.Combine(p, "Skyrim.esm"))) return p;
    if (Path.GetFileName(p).Equals("Data", StringComparison.OrdinalIgnoreCase)) return p;
    return null;
}

static string? FindCcc(string dataDir)
{
    foreach (var cand in new[]
    {
        Path.Combine(dataDir, "Skyrim.ccc"),
        Path.Combine(Path.GetDirectoryName(dataDir) ?? "", "Skyrim.ccc"),
    })
        if (File.Exists(cand)) return cand;
    return null;
}

// MO2-style plugin resolution: Data dir first, then the mods staging folders
// (later mods override earlier; files at the mod root or under Data/).
static string? ResolvePlugin(string name, string dataDir, string? modsDir)
{
    var inData = Path.Combine(dataDir, name);
    if (File.Exists(inData)) return inData;
    if (!string.IsNullOrEmpty(modsDir) && Directory.Exists(modsDir))
    {
        foreach (var mod in Directory.EnumerateDirectories(modsDir).OrderBy(d => d, StringComparer.OrdinalIgnoreCase))
        {
            foreach (var cand in new[] { Path.Combine(mod, name), Path.Combine(mod, "Data", name) })
                if (File.Exists(cand)) return cand;
        }
    }
    return null;
}

dataDir = ResolveDataDir(dataDir) ?? throw new ArgumentException($"Skyrim data directory not found: {dataDir}");
cccPath = cccPath.Length > 0 ? Path.GetFullPath(cccPath) : FindCcc(dataDir) ?? "";
if (cccPath.Length > 0) Console.WriteLine($"Skyrim.ccc: {cccPath}");
if (pluginsPath.Length > 0 && !File.Exists(pluginsPath))
    throw new ArgumentException($"plugins.txt not found: {pluginsPath}");

// ---- load order: vanilla masters (0-4), then CC files from Skyrim.ccc, then
// active plugins from plugins.txt. ESL files share the 0xFE space: each gets a
// light index (order among ESLs) and its full formIDs are
// (0xFE << 24) | (lightIndex << 12) | local12.
var files = new List<(string Name, int Index, bool Light, int LightIndex)>();
var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
var lightIndexCounter = 0;
var vanillaIndexes = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase)
{
    ["Skyrim.esm"] = 0,
    ["Update.esm"] = 1,
    ["Dawnguard.esm"] = 2,
    ["HearthFires.esm"] = 3,
    ["Dragonborn.esm"] = 4,
};
foreach (var name in vanillaIndexes.Keys)
{
    if (ResolvePlugin(name, dataDir, modsDir) != null)
    {
        files.Add((name, vanillaIndexes[name], false, -1));
        seen.Add(name);
    }
}
var nextIndex = 5;
if (cccPath.Length > 0)
{
    foreach (var line in File.ReadAllLines(cccPath))
    {
        var name = line.Trim();
        if (name.Length == 0 || seen.Contains(name)) continue;
        var light = name.EndsWith(".esl", StringComparison.OrdinalIgnoreCase);
        files.Add((name, light ? 0xFE : nextIndex++, light, light ? lightIndexCounter++ : -1));
        seen.Add(name);
    }
}
// Active plugins from a MO2 plugins.txt (lines starting with '*'), in order.
if (pluginsPath.Length > 0)
{
    foreach (var line in File.ReadAllLines(pluginsPath))
    {
        var name = line.Trim().TrimStart('*');
        if (name.Length == 0 || name.StartsWith('#')) continue;
        if (seen.Contains(name)) continue;
        var light = name.EndsWith(".esl", StringComparison.OrdinalIgnoreCase);
        files.Add((name, light ? 0xFE : nextIndex++, light, light ? lightIndexCounter++ : -1));
        seen.Add(name);
    }
}

static uint FullFormId(int modIndex, bool light, int lightIndex, FormKey fk)
{
    var id = fk.ID;
    if (light)
    {
        return (0xFEu << 24) | ((uint)lightIndex << 12) | (id & 0xFFF);
    }
    return ((uint)modIndex << 24) | (id & 0xFFFFFF);
}

// Getter-interface names of the record types the EverythingRandomizer shuffles.
var domainTypes = new[] { "ILeveledItemGetter", "ILeveledNpcGetter", "ILeveledSpellGetter", "IFormListGetter", "IContainerGetter", "IIngredientGetter", "IAlchemyItemGetter", "IObjectEffectGetter", "IShoutGetter", "ILightGetter", "IWeaponGetter", "IArmorGetter", "INpcGetter" };
var questAliasRefs = new SortedSet<uint>();
var questScriptRefs = new SortedSet<uint>();
var dobjRefs = new SortedSet<uint>();
var formInfo = new SortedDictionary<uint, (string Type, string EditorId)>();
var recordCensus = new SortedDictionary<string, long>();
var domainCensus = new SortedDictionary<string, long>();
long lvliEntries = 0, contEntries = 0, flstEntries = 0;

// ---- VMAD script-reference (SCRO) parsing ----
//
// VMAD layout: version(2) obScriptCount(1)
//   [ name(zstring16) flags(1) propCount(2) props... ]
// fragmentCount(1) [ fileName(zstring16) flags(1) scriptName(zstring16) fragmentName(zstring16) ]
// refCount(4) refs(refCount * formID)
//
// Property values (type byte): 1 Object(4) 2 String 3 Int(4) 4 Float(4)
// 5 Bool(1) 6/8/9/10 arrays of the above 7 array of String 11 Struct
// 12 array of Struct. Struct variables carry no name (type + flags + value).
// Returns the trailing formIDs or null when malformed.

static ushort RdU16(byte[] b, ref int p) { var v = (ushort)(b[p] | b[p + 1] << 8); p += 2; return v; }
static uint RdU32(byte[] b, ref int p) { var v = (uint)(b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24); p += 4; return v; }

static bool SkipValue(byte[] b, ref int p, byte ty)
{
    switch (ty)
    {
        case 1:
            if (p + 4 > b.Length) return false;
            p += 4;
            return true;
        case 2:
            {
                if (p + 2 > b.Length) return false;
                var len = RdU16(b, ref p);
                if (p + len > b.Length) return false;
                p += len;
                return true;
            }
        case 3:
        case 4:
            if (p + 4 > b.Length) return false;
            p += 4;
            return true;
        case 5:
            if (p + 1 > b.Length) return false;
            p += 1;
            return true;
        case 6:
        case 8:
        case 9:
        case 10:
            {
                if (p + 4 > b.Length) return false;
                var count = (int)RdU32(b, ref p);
                var elem = ty == 6 || ty == 8 || ty == 9 ? 4u : 1u;
                if (p + count * elem > b.Length) return false;
                p += count * (int)elem;
                return true;
            }
        case 7:
            {
                if (p + 4 > b.Length) return false;
                var count = (int)RdU32(b, ref p);
                for (var i = 0; i < count; i++)
                {
                    if (p + 2 > b.Length) return false;
                    var len = RdU16(b, ref p);
                    if (p + len > b.Length) return false;
                    p += len;
                }
                return true;
            }
        case 11:
            {
                if (p + 2 > b.Length) return false;
                var vars = RdU16(b, ref p);
                for (var i = 0; i < vars; i++)
                {
                    if (p + 2 > b.Length) return false;
                    var t = b[p++];
                    p++; // flags
                    if (!SkipValue(b, ref p, t)) return false;
                }
                return true;
            }
        case 12:
            {
                if (p + 4 > b.Length) return false;
                var count = (int)RdU32(b, ref p);
                for (var i = 0; i < count; i++)
                {
                    if (p + 2 > b.Length) return false;
                    var vars = RdU16(b, ref p);
                    for (var j = 0; j < vars; j++)
                    {
                        if (p + 2 > b.Length) return false;
                        var t = b[p++];
                        p++;
                        if (!SkipValue(b, ref p, t)) return false;
                    }
                }
                return true;
            }
        default:
            return false;
    }
}

static List<uint>? ParseVmad(byte[] data)
{
    if (data.Length < 3) return null;
    var p = 0;
    RdU16(data, ref p);
    var scriptCount = data[p++];
    for (var i = 0; i < scriptCount; i++)
    {
        if (p + 2 > data.Length) return null;
        var nameLen = RdU16(data, ref p);
        if (p + nameLen > data.Length) return null;
        p += nameLen;
        if (p + 1 > data.Length) return null;
        p++; // flags
        if (p + 2 > data.Length) return null;
        var propCount = RdU16(data, ref p);
        for (var j = 0; j < propCount; j++)
        {
            if (p + 2 > data.Length) return null;
            var propNameLen = RdU16(data, ref p);
            if (p + propNameLen > data.Length) return null;
            p += propNameLen;
            if (p + 2 > data.Length) return null;
            var ty = data[p++];
            p++; // flags
            if (!SkipValue(data, ref p, ty)) return null;
        }
    }
    if (p + 1 > data.Length) return null;
    var fragCount = data[p++];
    for (var i = 0; i < fragCount; i++)
    {
        for (var part = 0; part < 4; part++)
        {
            if (p + 2 > data.Length) return null;
            var len = RdU16(data, ref p);
            if (p + len > data.Length) return null;
            p += len;
        }
    }
    if (p + 4 > data.Length) return null;
    var refCount = (int)RdU32(data, ref p);
    if (p + refCount * 4 > data.Length) return null;
    var refs = new List<uint>(refCount);
    for (var i = 0; i < refCount; i++)
    {
        if (p + 4 > data.Length) return null;
        refs.Add(RdU32(data, ref p));
    }
    return refs;
}

// Raw record data of a BinaryOverlay record (PluginBinaryOverlay._recordData).
// Reflection: the base classes expose no public raw access.
static byte[] RawRecordData(object record)
{
    var field = record.GetType().BaseType!.BaseType!.BaseType!
        .GetField("_recordData", BindingFlags.NonPublic | BindingFlags.Instance)!;
    var slice = field.GetValue(record)!;
    var toArray = slice.GetType().GetMethod("ToArray")!;
    return (byte[])toArray.Invoke(slice, null)!;
}

static List<uint>? QuestScriptRefsFromVmad(object quest)
{
    var data = RawRecordData(quest);
    var p = 0;
    while (p + 6 <= data.Length)
    {
        var type = Encoding.ASCII.GetString(data, p, 4);
        p += 4; // advance past the type before reading the size
        var size = RdU16(data, ref p);
        if (p + size > data.Length) return null;
        if (type == "VMAD")
        {
            var body = new byte[size];
            Array.Copy(data, p, body, 0, size);
            return ParseVmad(body);
        }
        p += size;
    }
    return null;
}

// Raw alias subrecord scan (fallback net): the typed model only exposes the
// alias fields whose declared reference type matches the target (e.g. ALLS is
// typed as a FormList link), so references that point at unexpected record
// types (e.g. ALLS -> TreasBag, a container) are dropped by Mutagen. Scanning
// the raw ALFR/ALFL/ALFA/ALLS subrecords recovers those.
static void RawAliasRefs(object quest, HashSet<uint> outRefs)
{
    var data = RawRecordData(quest);
    var p = 0;
    while (p + 6 <= data.Length)
    {
        var type = Encoding.ASCII.GetString(data, p, 4);
        p += 4;
        var size = RdU16(data, ref p);
        if (p + size > data.Length) return;
        if (type is "ALFR" or "ALFL" or "ALFA" or "ALLS")
        {
            if (size == 4)
            {
                var v = (uint)(data[p] | data[p + 1] << 8 | data[p + 2] << 16 | data[p + 3] << 24);
                if (v != 0) outRefs.Add(v);
            }
            else if (size == 6)
            {
                // u16 aliasID + u32 formID layout
                var v = (uint)(data[p + 2] | data[p + 3] << 8 | data[p + 4] << 16 | data[p + 5] << 24);
                if (v != 0) outRefs.Add(v);
            }
        }
        p += size;
    }
}

var missing = new List<string>();
foreach (var (name, index, light, lightIndex) in files)
{
    var path = ResolvePlugin(name, dataDir, modsDir);
    if (path == null)
    {
        missing.Add(name);
        continue;
    }
    Console.WriteLine($"parsing {name} (index {index:X2}, light: {light} lightIndex: {lightIndex}) ...");
    using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
    uint Full(FormKey fk) => FullFormId(index, light, lightIndex, fk);

    // one pass: census + editorID map for every record
    var counts = new SortedDictionary<string, long>();
    var domainCounts = new SortedDictionary<string, long>();
    var editorIds = new Dictionary<uint, (string, string)>();
    foreach (var rec in mod.EnumerateMajorRecords())
    {
        var typeName = rec.Type.Name;
        counts.TryGetValue(typeName, out var n);
        counts[typeName] = n + 1;
        if (domainTypes.Contains(typeName))
        {
            domainCounts.TryGetValue(typeName, out var d);
            domainCounts[typeName] = d + 1;
        }
        var fid = Full(rec.FormKey);
        if (domainTypes.Contains(typeName) && !editorIds.ContainsKey(fid))
            editorIds[fid] = (typeName, rec.EditorID ?? "");
    }
    foreach (var (k, v) in counts)
    {
        recordCensus.TryGetValue(k, out var c);
        recordCensus[k] = c + v;
    }
    foreach (var (k, v) in domainCounts)
    {
        domainCensus.TryGetValue(k, out var c);
        domainCensus[k] = c + v;
    }
    foreach (var (k, v) in editorIds)
        formInfo.TryAdd(k, v);
    Console.WriteLine($"  records: {counts.Values.Sum()}");
    foreach (var (k, v) in counts.Where(kv => kv.Value > 0))
        Console.WriteLine($"    {k}: {v}");

    // ---- quest alias references + quest script refs (SCRO) ----
    var questCount = 0;
    foreach (var quest in mod.EnumerateMajorRecords<IQuestGetter>())
    {
        questCount++;
        // Raw scan first: recovers alias refs Mutagen's typed model drops
        // (wrong reference type) and the SCRO script refs it does not expose.
        var rawAliasRefs = new HashSet<uint>();
        RawAliasRefs(quest, rawAliasRefs);
        foreach (var r in rawAliasRefs)
            questAliasRefs.Add(Full(new FormKey(quest.FormKey.ModKey, r)));
        var questRefs = QuestScriptRefsFromVmad(quest);
        if (questRefs != null)
            foreach (var r in questRefs)
                if (r != 0)
                    questScriptRefs.Add(Full(new FormKey(quest.FormKey.ModKey, r)));
        if (quest.Aliases == null) continue;
        foreach (var alias in quest.Aliases)
        {
            void AddNullable<T>(IFormLinkNullableGetter<T> link) where T : class, IMajorRecordGetter
            {
                if (!link.IsNull) questAliasRefs.Add(Full(link.FormKey));
            }
            void AddMany<T>(IEnumerable<IFormLinkGetter<T>> links) where T : class, IMajorRecordGetter
            {
                foreach (var l in links) questAliasRefs.Add(Full(l.FormKey));
            }
            AddNullable(alias.ForcedReference);
            AddNullable(alias.SpecificLocation);
            AddNullable(alias.UniqueActor);
            if (alias.Location != null)
                AddNullable(alias.Location.Keyword);
            AddMany(alias.Factions ?? Enumerable.Empty<IFormLinkGetter<IFactionGetter>>());
            AddMany(alias.Spells ?? Enumerable.Empty<IFormLinkGetter<ISpellGetter>>());
            AddMany(alias.Keywords ?? Enumerable.Empty<IFormLinkGetter<IKeywordGetter>>());
            AddMany(alias.PackageData ?? Enumerable.Empty<IFormLinkGetter<IPackageGetter>>());
            if (alias.Items != null)
                foreach (var entry in alias.Items)
                    if (entry.Item.Item.FormKey != FormKey.Null)
                        questAliasRefs.Add(Full(entry.Item.Item.FormKey));
            AddNullable(alias.CombatOverridePackageList);
            AddNullable(alias.GuardWarnOverridePackageList);
            AddNullable(alias.ObserveDeadBodyOverridePackageList);
            AddNullable(alias.SpectatorOverridePackageList);
        }
    }
    Console.WriteLine($"  quests: {questCount}, alias refs so far: {questAliasRefs.Count}, script refs so far: {questScriptRefs.Count}");

    // ---- DOBJ ----
    foreach (var dobj in mod.EnumerateMajorRecords<IDefaultObjectManagerGetter>())
    {
        if (dobj.Objects == null) continue;
        foreach (var o in dobj.Objects)
        {
            if (o.Object.FormKey != FormKey.Null)
                dobjRefs.Add(Full(o.Object.FormKey));
        }
    }

    // ---- domain entry counts ----
    foreach (var lvli in mod.EnumerateMajorRecords<ILeveledItemGetter>())
        lvliEntries += lvli.Entries?.Count ?? 0;
    foreach (var lvlc in mod.EnumerateMajorRecords<ILeveledNpcGetter>())
        lvliEntries += lvlc.Entries?.Count ?? 0;
    foreach (var lvsp in mod.EnumerateMajorRecords<ILeveledSpellGetter>())
        lvliEntries += lvsp.Entries?.Count ?? 0;
    foreach (var cont in mod.EnumerateMajorRecords<IContainerGetter>())
        contEntries += cont.Items?.Count ?? 0;
    foreach (var flst in mod.EnumerateMajorRecords<IFormListGetter>())
        flstEntries += flst.Items?.Count ?? 0;
}
if (missing.Count > 0)
    Console.WriteLine($"missing {missing.Count}: {string.Join(", ", missing)}");

// ---- output ----
static string JsonEscape(string s)
{
    var sb = new StringBuilder(s.Length + 2);
    sb.Append('"');
    foreach (var ch in s)
    {
        switch (ch)
        {
            case '"': sb.Append("\\\""); break;
            case '\\': sb.Append("\\\\"); break;
            case '\n': sb.Append("\\n"); break;
            case '\r': sb.Append("\\r"); break;
            case '\t': sb.Append("\\t"); break;
            default:
                if (ch < 0x20) sb.Append($"\\u{(int)ch:x4}");
                else sb.Append(ch);
                break;
        }
    }
    sb.Append('"');
    return sb.ToString();
}

static void EmitList(StringBuilder sb, string name, IEnumerable<uint> set)
{
    sb.Append($"  {JsonEscape(name)}: [");
    var first = true;
    foreach (var id in set)
    {
        if (!first) sb.Append(", ");
        first = false;
        sb.Append(JsonEscape(id.ToString("X8")));
    }
    sb.Append("],\n");
}

static void EmitLua(StringBuilder sb, SortedSet<uint> protectedSet, SortedDictionary<uint, (string Type, string EditorId)> info)
{
    sb.Append("-- Auto-generated by third_party/protectgen. Do not edit.\n");
    sb.Append("-- Protected forms: quest alias references and default-object\n");
    sb.Append("-- (DOBJ) forms from the vanilla + Creation Club masters.\n");
    sb.Append("-- Loaded via loadLua.\n");
    sb.Append("return {\n");
    sb.Append("  protected = {\n");
    var ids = protectedSet.ToArray();
    for (var i = 0; i < ids.Length; i += 10)
    {
        sb.Append("    ");
        for (var j = i; j < Math.Min(i + 10, ids.Length); j++)
        {
            if (j > i) sb.Append(", ");
            sb.Append($"[{ids[j]}] = true");
        }
        sb.Append(",\n");
    }
    sb.Append("  },\n");
    sb.Append("  info = {\n");
    foreach (var (fid, val) in info)
    {
        var label = string.IsNullOrEmpty(val.EditorId) ? val.Type : val.EditorId;
        sb.Append($"    [{fid}] = {JsonEscape(label)},\n");
    }
    sb.Append("  },\n");
    sb.Append("}\n");
}

var protectedSet = new SortedSet<uint>(questAliasRefs);
protectedSet.UnionWith(questScriptRefs);
protectedSet.UnionWith(dobjRefs);

if (outPath.Length == 0)
    outPath = Path.Combine(Directory.GetCurrentDirectory(), "EverythingRandomizer_protection.lua");
else
    outPath = Path.GetFullPath(outPath);

var lua = new StringBuilder();
EmitLua(lua, protectedSet, formInfo);
File.WriteAllText(outPath, lua.ToString());
Console.WriteLine($"wrote {outPath}");

if (jsonPath.Length > 0)
{
    var json = new StringBuilder();
    json.Append("{\n");
    EmitList(json, "questAliasRefs", questAliasRefs);
    EmitList(json, "questScriptRefs", questScriptRefs);
    EmitList(json, "questItemForms", Array.Empty<uint>());
    EmitList(json, "dobjRefs", dobjRefs);
    json.Append("  \"formInfo\": {\n");
    var first2 = true;
    foreach (var (id, val) in formInfo)
    {
        if (!first2) json.Append(",\n");
        first2 = false;
        json.Append($"    {JsonEscape(id.ToString("X8"))}: {{\"type\": {JsonEscape(val.Type)}, \"editorId\": {JsonEscape(val.EditorId)}}}");
    }
    json.Append("\n  }\n}\n");
    File.WriteAllText(Path.GetFullPath(jsonPath), json.ToString());
    Console.WriteLine($"wrote {jsonPath}");
}

// ---- danger zone summary ----
Console.WriteLine("\n== combined danger zone (Mutagen) ==");
foreach (var t in domainTypes)
{
    var n = formInfo.Count(kv => protectedSet.Contains(kv.Key) && kv.Value.Type == t);
    if (n > 0)
    {
        Console.WriteLine($"  protected {t}: {n}");
        foreach (var ex in formInfo.Where(kv => protectedSet.Contains(kv.Key) && kv.Value.Type == t).Take(8))
            Console.WriteLine($"      e.g. {ex.Key:X8} {ex.Value.EditorId}");
    }
}
Console.WriteLine($"  questAliasRefs: {questAliasRefs.Count}");
Console.WriteLine($"  questScriptRefs: {questScriptRefs.Count}");
Console.WriteLine($"  dobjRefs: {dobjRefs.Count}");
Console.WriteLine($"  total protected: {protectedSet.Count}");
Console.WriteLine($"  domain totals: LVLI entries {lvliEntries} CONT entries {contEntries} FLST entries {flstEntries}");
return 0;

// ---- native Windows shell dialog (P/Invoke; Windows-only by construction) ----
internal static class NativeDialogs
{
    public const uint BIF_RETURNONLYFSDIRS = 0x0001;
    public const uint BIF_NEWDIALOGSTYLE = 0x0040;
    public const uint BIF_BROWSEINCLUDEFILES = 0x4000;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct BROWSEINFO
    {
        public IntPtr hwndOwner;
        public IntPtr pidlRoot;
        public string? pszDisplayName;
        public string lpszTitle;
        public uint ulFlags;
        public IntPtr lpfn;
        public IntPtr lParam;
        public int iImage;
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SHBrowseForFolder(ref BROWSEINFO lpbi);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SHGetPathFromIDList(IntPtr pidl, StringBuilder pszPath);
}