// Source: WOOSEOK99/S8RPKCheats 068ca1d881edf870f7cf06f935cf50e286004af8.
// Native UI machine images, guards and relocations from TraitViewer*.cpp/.inc.
// Runtime ownership, admission and rollback are implemented in trait_viewer_native.cpp.

namespace roster_data {
constexpr std::array<std::byte, 235> bytes_0 = native_hex(
    "4C8BDC49895B1049897318574881EC800000008B3D17501901488D05D847BE0049C743B000000000498D53B8498943B841B8D7000000498D"
    "43B866C744242C0000498943F0488BD9897C2428E89F14B5FE488D4C24204889442420488BF048895C2430E89808030084C07405488BC6EB"
    "654C8B057876180183FF0175204138780C754C4D85C0744741807814007540410FB7400A4869C8F4000000EB1F4180780C01752B4D85C074"
    "264180781400751F410FB7400A4869C8C9010000418B50104803CA428B04014803C24903C0EB07488B05E2D412014C8D9C2480000000498B"
    "5B18498B7320498BE35FC3");
constexpr std::array<std::byte, 50> bytes_1 = native_hex(
    "40534883EC60488D05F347BE00488BD948894424204C8D442420488D4424204889442458E86711B5FE488BC34883C4605BC3");
constexpr std::array<std::byte, 13> bytes_2 = native_hex(
    "488B0ABA02000000E903434A01");
constexpr std::array<std::byte, 38> bytes_3 = native_hex(
    "40534883EC20498BC0488BDA41B802000000488BCB488B10E893434A01488BC34883C4205BC3");
constexpr std::array<std::byte, 8> bytes_4 = native_hex(
    "30AEACC733000000");
constexpr std::array<std::byte, 56> bytes_5 = native_hex(
    "000000000500000007000000A400000018000000190000001A0000001B0000001C000000AB000000E60000007300000074000000FFFFFFFF");
constexpr std::array<std::byte, 8> bytes_6 = native_hex(
    "7500000000000000");
constexpr std::array<std::byte, 8> bytes_7 = native_hex(
    "9200000001000000");
constexpr std::array<std::byte, 8> bytes_8 = native_hex(
    "FFFF000001000000");
constexpr std::array<std::byte, 5> bytes_9 = native_hex(
    "48895C2408");
constexpr std::array<std::byte, 13> bytes_10 = native_hex(
    "488B0ABA01000000E903434A01");
constexpr std::array<std::byte, 38> bytes_11 = native_hex(
    "40534883EC20498BC0488BDA41B801000000488BCB488B10E893434A01488BC34883C4205BC3");
constexpr std::array<std::byte, 56> bytes_12 = native_hex(
    "0000000099000000980000000900000005000000A400000018000000190000001A000000E60000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 44> bytes_13 = native_hex(
    "000000009800000018000000190000001A0000001B0000001C0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 56> bytes_14 = native_hex(
    "000000001A00000099000000980000000900000005000000A40000001800000019000000E60000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 52> bytes_15 = native_hex(
    "000000000500000007000000A400000018000000190000001A0000001B0000001C0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 52> bytes_16 = native_hex(
    "CE0000000500000007000000A400000018000000190000001A0000001B0000001C0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 52> bytes_17 = native_hex(
    "000000000700000005000000A400000018000000190000001A0000001B0000001C0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 44> bytes_18 = native_hex(
    "000000000500000018000000190000001A0000001B0000001C0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 52> bytes_19 = native_hex(
    "000000002300000034000000930000009B0000009C0000009D0000009E0000009F0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 48> bytes_20 = native_hex(
    "0000000023000000920000009B0000009C0000009D0000009E0000009F0000007300000074000000FFFFFFFFFFFFFFFF");
constexpr std::array<std::byte, 4> bytes_21 = native_hex(
    "FFFFFFFF");
constexpr std::array<std::byte, 4> bytes_22 = native_hex(
    "75000000");
constexpr std::array<std::byte, 4> bytes_23 = native_hex(
    "01000000");
constexpr std::array<std::byte, 4> bytes_24 = native_hex(
    "01000020");
constexpr std::array<ViewerBytes, 5> pieces = {{
    {0x0, bytes_0},
    {0x100, bytes_1},
    {0x180, bytes_2},
    {0x1A0, bytes_3},
    {0x250, bytes_4},
}};
constexpr std::array<ViewerBytes, 22> guards = {{
    {0x3A6C0F0, bytes_5},
    {0x2C71CE8, bytes_6},
    {0x2C71CF8, bytes_7},
    {0x2C71D30, bytes_8},
    {0x1AE5BA0, bytes_9},
    {0x1ADCCB0, bytes_0},
    {0x1ADCC70, bytes_1},
    {0x641890, bytes_10},
    {0x6418A0, bytes_11},
    {0x3A74B38, bytes_12},
    {0x3A7DA30, bytes_13},
    {0x3A79188, bytes_14},
    {0x3ACCE28, bytes_15},
    {0x3B0A688, bytes_15},
    {0x3B019E8, bytes_15},
    {0x3B1C220, bytes_16},
    {0x3ABFB38, bytes_17},
    {0x3ABB4E8, bytes_15},
    {0x3ADE768, bytes_15},
    {0x3A82080, bytes_18},
    {0x3B360F8, bytes_19},
    {0x3B3ED98, bytes_20},
}};
constexpr std::array<ViewerCopy, 4> copies = {{
    {0x200, 0x26C14A8, 0x10},
    {0x220, 0x26C14C8, 0x8},
    {0x228, 0x26C1470, 0x10},
    {0x240, 0x26C1488, 0x10},
}};
constexpr std::array<ViewerPointerGuard, 6> pointer_guards = {{
    {0x362AA90, 0x3A6C0F0},
    {0x362B8A0, 0x3A82080},
    {0x3632BD8, 0x3B360F8},
    {0x3633178, 0x3B3ED98},
    {0x2C71D00, 0x1ADCC60},
    {0x2C71D10, 0x1ADCC20},
}};
constexpr std::array<ViewerRelocation, 13> relocations = {{
    {0x15, 0x2C71CE0, ViewerAddress::Module, false},
    {0x1C, 0x200, ViewerAddress::Cave, false},
    {0x4D, 0x62E1A0, ViewerAddress::Module, false},
    {0x64, 0x1B0D5B0, ViewerAddress::Module, false},
    {0x74, 0x2C643A0, ViewerAddress::Module, false},
    {0xD2, 0x2C0A268, ViewerAddress::Module, false},
    {0x109, 0x228, ViewerAddress::Cave, false},
    {0x125, 0x62DE00, ViewerAddress::Module, false},
    {0x189, 0x1AE5BA0, ViewerAddress::Module, false},
    {0x1B9, 0x1AE5C50, ViewerAddress::Module, false},
    {0x210, 0x180, ViewerAddress::Cave, true},
    {0x218, 0x1A0, ViewerAddress::Cave, true},
    {0x238, 0x1A0, ViewerAddress::Cave, true},
}};
constexpr std::array<ViewerPatch, 18> patches = {{
    {0x3A74B68, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3A7DA54, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3A791B8, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3ACCE54, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3B0A6B4, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3B01A14, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3B1C24C, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3ABFB64, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3ABB514, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3ADE794, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3A820A4, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3B36124, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x3B3EDC0, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
    {0x2C71CF0, ViewerPatchKind::Pointer, {}, {}, 0x250, ViewerAddress::Cave},
    {0x2C71D00, ViewerPatchKind::Pointer, {}, {}, 0x0, ViewerAddress::Cave},
    {0x2C71D10, ViewerPatchKind::Pointer, {}, {}, 0x100, ViewerAddress::Cave},
    {0x2C71CFC, ViewerPatchKind::Bytes, bytes_23, bytes_24, 0x0, ViewerAddress::Cave},
    {0x3A6C124, ViewerPatchKind::Bytes, bytes_21, bytes_22, 0x0, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x1ADCC60, 0x1000, false,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace roster_data

namespace widths_data {
constexpr std::array<std::byte, 8> bytes_0 = native_hex(
    "9900000000000000");
constexpr std::array<std::byte, 8> bytes_1 = native_hex(
    "0500000000000000");
constexpr std::array<std::byte, 4> bytes_2 = native_hex(
    "BC000000");
constexpr std::array<std::byte, 4> bytes_3 = native_hex(
    "A0000000");
constexpr std::array<std::byte, 4> bytes_4 = native_hex(
    "68000000");
constexpr std::array<std::byte, 4> bytes_5 = native_hex(
    "4C000000");
constexpr std::array<ViewerBytes, 0> pieces = {{
}};
constexpr std::array<ViewerBytes, 2> guards = {{
    {0x2C72948, bytes_0},
    {0x2C6F668, bytes_1},
}};
constexpr std::array<ViewerCopy, 0> copies = {{
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 0> relocations = {{
}};
constexpr std::array<ViewerPatch, 2> patches = {{
    {0x2C72958, ViewerPatchKind::Bytes, bytes_2, bytes_3, 0x0, ViewerAddress::Cave},
    {0x2C6F678, ViewerPatchKind::Bytes, bytes_4, bytes_5, 0x0, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x0, 0x0, false,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace widths_data

namespace battle_prep_data {
constexpr std::array<std::byte, 170> bytes_0 = native_hex(
    "9C50515253555657415041514152415341544155415641574881EC80000000F30F7F442420F30F7F4C2430F30F7F542440F30F7F5C2450F3"
    "0F7F642460F30F7F6C24704C89F9488B55884C8B8424700100004C8B4D98E8A5030000F30F6F442420F30F6F4C2430F30F6F542440F30F6F"
    "5C2450F30F6F642460F30F6F6C24704881C480000000415F415E415D415C415B415A415941585F5E5D5B5A59589D498DBF60070000E90000"
    "0000");
constexpr std::array<std::byte, 643> bytes_1 = native_hex(
    "5355565741544155415641574881EC880000004989CF48895424604C894424684C894C2470F048FF05DBEBFFFF4C8D35CCECFFFF31DBB910"
    "0000004D393E743E49833E0075084885DB75034C89F34983C640FFC975E54885DB0F84080200004989DE4D893E488D059C1B000049890749"
    "8B87B0050000488B4020498946204531ED4B837CEE08000F85AD000000E800000000488B8818010000488B01BAB801000048C74424403500"
    "000048C7442448000000004C8D442440FF50284885C00F84A30100004889C34889C1E800000000488D0532F3FFFF4889034B895CEE08F048"
    "FF051AEBFFFF4889D9BA05000000F3450F2C46204181C03C010000F3450F2C4E244C897C242048C74424280000000048C744243001000000"
    "E8000000004889D9488B01FF5048498B8FA80500004885C974084889DAE80000000041FFC54183FD030F823AFFFFFF4531ED4531E44183FC"
    "027412438D442D004401E0498B9CC7B0050000EB054B8B5CEE084885DB0F84F4000000488D0586F5FFFFF3420F1004A0F3410F584620F30F"
    "11442450488D057DF5FFFFF3420F1004A8F3410F584624F30F114424544889D9488D542450488B03FF909800000031F64A8B7CEC604885FF"
    "742A488B074889F9FF504884C0741D4A8BBCE7880000004885FF7410488B074889F9FF504884C074034889FE4889D94889F24531C0E80000"
    "00004885F67434488B05000000004885C0740B0FB680880100003CFF7505E8000000004531C03C01740641B8010000004889D94889F2E800"
    "00000031D24885F67405BA010000004889D9488B03FF900801000041FFC44183FC030F82F5FEFFFF41FFC54183FD030F82E5FEFFFFEB08F0"
    "48FF05A1E9FFFF4881C488000000415F415E415D415C5F5E5D5BC3");
constexpr std::array<std::byte, 116> bytes_2 = native_hex(
    "9C50415041514C8D05F3DCFFFF41B91000000049394808741749394810741B49394818741F4983C04041FFC975E5EB3949C7400800000000"
    "EB1249C7401000000000EB0849C7401800000000F048FF0DACDBFFFF498B4008490B4010490B4018750749C7000000000041594158589DE9"
    "00000000");
constexpr std::array<std::byte, 115> bytes_3 = native_hex(
    "5356574883EC404889CF48895424204C894424284C894C2430E8000000004889442438488D1DD6C0FFFFBE1000000048393B740A4883C340"
    "FFCE75F3EB2831F6488B4CF3084885C97415488B01488B5424204C8B4424284C8B4C2430FF5020FFC683FE0372DA488B4424384883C4405F"
    "5E5BC3");
constexpr std::array<std::byte, 115> bytes_4 = native_hex(
    "5356574883EC404889CF48895424204C894424284C894C2430E8000000004889442438488D1DD6BCFFFFBE1000000048393B740A4883C340"
    "FFCE75F3EB2831F6488B4CF3084885C97415488B01488B5424204C8B4424284C8B4C2430FF5028FFC683FE0372DA488B4424384883C4405F"
    "5E5BC3");
constexpr std::array<std::byte, 111> bytes_5 = native_hex(
    "5356574883EC404889CF48895424204C894424284C894C2430488D1DE0B8FFFFBE1000000048393B740A4883C340FFCE75F3EB1D31F6488B"
    "4CF3084885C9740A488B01BA01000000FF10FFC683FE0372E54889F9488B5424204C8B4424284C8B4C24304883C4405F5E5BE900000000");
constexpr std::array<std::byte, 167> bytes_6 = native_hex(
    "4885D20F84940000004C8D15F0B0FFFF41BB1000000049390A740B4983C24041FFCB75F2EB7749395208740C493952107406493952187565"
    "5356574883EC20488BB1A00500004885F6744A31FF488B9AB00100004885DB742E4889D9488B01FF504884C074214889D9E8000000004885"
    "C074144889C24889F1488B01FF9040020000BF010000004889F189FA488B01FF90080100004883C4205F5E5BC3564883EC20E900000000");
constexpr std::array<std::byte, 126> bytes_7 = native_hex(
    "5356574883EC404889CF48895424204C894424284C894C2430E8000000004889442438488D1DD6ACFFFFBE1000000048393B740A4883C340"
    "FFCE75F3EB3331F6488B4CF3084885C97420488B01488B5424204C8B4424284C8B4C2430FF5050488B4CF308488B01FF5048FFC683FE0372"
    "CF488B4424384883C4405F5E5BC3");
constexpr std::array<std::byte, 115> bytes_8 = native_hex(
    "5356574883EC404889CF48895424204C894424284C894C2430E8000000004889442438488D1DD6A8FFFFBE1000000048393B740A4883C340"
    "FFCE75F3EB2831F6488B4CF3084885C97415488B01488B5424204C8B4424284C8B4C2430FF5050FFC683FE0372DA488B4424384883C4405F"
    "5E5BC3");
constexpr std::array<std::byte, 170> bytes_9 = native_hex(
    "9C50515253555657415041514152415341544155415641574881EC80000000F30F7F442420F30F7F4C2430F30F7F542440F30F7F5C2450F3"
    "0F7F642460F30F7F6C2470488B8BA00500004885C9740EBA03000000488B01FF9080000000F30F6F442420F30F6F4C2430F30F6F542440F3"
    "0F6F5C2450F30F6F642460F30F6F6C24704881C480000000415F415E415D415C415B415A415941585F5E5D5B5A59589DB990000000E90000"
    "0000");
constexpr std::array<std::byte, 12> bytes_10 = native_hex(
    "0000000000001E4300009E43");
constexpr std::array<std::byte, 12> bytes_11 = native_hex(
    "000000C10000F04100008842");
constexpr std::array<std::byte, 5> bytes_12 = native_hex(
    "B990000000");
constexpr std::array<std::byte, 6> bytes_13 = native_hex(
    "40564883EC20");
constexpr std::array<std::byte, 5> bytes_14 = native_hex(
    "48895C2408");
constexpr std::array<std::byte, 7> bytes_15 = native_hex(
    "498DBF60070000");
constexpr std::array<std::byte, 9> bytes_16 = native_hex(
    "40534883EC20488BD9");
constexpr std::array<std::byte, 7> bytes_17 = native_hex(
    "4883EC484C8BD9");
constexpr std::array<ViewerBytes, 12> pieces = {{
    {0x1000, bytes_0},
    {0x1400, bytes_1},
    {0x2400, bytes_2},
    {0x4000, bytes_3},
    {0x4400, bytes_4},
    {0x4800, bytes_5},
    {0x5000, bytes_6},
    {0x5400, bytes_7},
    {0x5800, bytes_8},
    {0x5C00, bytes_9},
    {0xB00, bytes_10},
    {0xB10, bytes_11},
}};
constexpr std::array<ViewerBytes, 6> guards = {{
    {0x1A18B41, bytes_12},
    {0x19ED9F0, bytes_13},
    {0x1EF5F40, bytes_14},
    {0x1A1B52C, bytes_15},
    {0x162ECA0, bytes_16},
    {0x161AF00, bytes_17},
}};
constexpr std::array<ViewerCopy, 6> copies = {{
    {0x800, 0x25EE268, 0x8},
    {0x810, 0x25EE278, 0x260},
    {0x3000, 0x26B0808, 0x8},
    {0x3010, 0x26B0818, 0x18},
    {0x3038, 0x26B0840, 0x18},
    {0x3060, 0x26B0868, 0x3A8},
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 24> relocations = {{
    {0x10A6, 0x1A1B533, ViewerAddress::Module, false},
    {0x148E, 0x14ED0, ViewerAddress::Module, false},
    {0x14CB, 0x162ECA0, ViewerAddress::Module, false},
    {0x1519, 0x161AF00, ViewerAddress::Module, false},
    {0x1536, 0x1EF5F40, ViewerAddress::Module, false},
    {0x15F6, 0x162EDA0, ViewerAddress::Module, false},
    {0x1602, 0x2E99C98, ViewerAddress::Module, false},
    {0x1617, 0x16AC280, ViewerAddress::Module, false},
    {0x162F, 0x162EEC0, ViewerAddress::Module, false},
    {0x2470, 0x1725E0, ViewerAddress::Module, false},
    {0x401A, 0x19EEA70, ViewerAddress::Module, false},
    {0x441A, 0x1EDEF10, ViewerAddress::Module, false},
    {0x486B, 0x619030, ViewerAddress::Module, false},
    {0x506A, 0x170ABB0, ViewerAddress::Module, false},
    {0x50A3, 0x19ED9F6, ViewerAddress::Module, false},
    {0x541A, 0x19EEB10, ViewerAddress::Module, false},
    {0x581A, 0x19EEAB0, ViewerAddress::Module, false},
    {0x5CA6, 0x1A18B46, ViewerAddress::Module, false},
    {0x808, 0x2400, ViewerAddress::Cave, true},
    {0x3008, 0x4800, ViewerAddress::Cave, true},
    {0x3028, 0x4000, ViewerAddress::Cave, true},
    {0x3030, 0x4400, ViewerAddress::Cave, true},
    {0x3050, 0x5400, ViewerAddress::Cave, true},
    {0x3058, 0x5800, ViewerAddress::Cave, true},
}};
constexpr std::array<ViewerPatch, 3> patches = {{
    {0x1A1B52C, ViewerPatchKind::Jump, bytes_15, {}, 0x1000, ViewerAddress::Cave},
    {0x19ED9F0, ViewerPatchKind::Jump, bytes_13, {}, 0x5000, ViewerAddress::Cave},
    {0x1A18B41, ViewerPatchKind::Jump, bytes_12, {}, 0x5C00, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x1A1B52C, 0x10000, true,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace battle_prep_data

namespace battle_map_data {
constexpr std::array<std::byte, 222> bytes_0 = native_hex(
    "5356574881EC500500004889CB48899424200500004C898424280500004C898C24300500004C8B9C24A00500004C89DE488D7C2440B94800"
    "0000FCF348A5498DB320010000B924000000F348A5498DB340020000B930000000F348A58B842490020000298424880200008B8424F00200"
    "00298424E80200008B84245003000029842448030000488B8424900500004889442420488B8424980500004889442428488D442440488944"
    "243048C74424380D0000004889D9488B9424200500004C8B8424280500004C8B8C2430050000E8000000004881C4500500005F5E5BC3");
constexpr std::array<std::byte, 63> bytes_1 = native_hex(
    "48C783080200000000000048C783100200000000000048C7831802000000000000488D0500000000488903F048FF05000000004889D84883"
    "C420E900000000");
constexpr std::array<std::byte, 13> bytes_2 = native_hex(
    "F048FF0D00000000E900000000");
constexpr std::array<std::byte, 5> bytes_3 = native_hex(
    "BA08020000");
constexpr std::array<std::byte, 5> bytes_4 = native_hex(
    "BA20020000");
constexpr std::array<std::byte, 4> bytes_5 = native_hex(
    "4883FD06");
constexpr std::array<std::byte, 4> bytes_6 = native_hex(
    "4883FD09");
constexpr std::array<std::byte, 3> bytes_7 = native_hex(
    "83FD06");
constexpr std::array<std::byte, 3> bytes_8 = native_hex(
    "83FD09");
constexpr std::array<std::byte, 5> bytes_9 = native_hex(
    "BF06000000");
constexpr std::array<std::byte, 5> bytes_10 = native_hex(
    "BF09000000");
constexpr std::array<std::byte, 7> bytes_11 = native_hex(
    "488D9EE8010000");
constexpr std::array<std::byte, 7> bytes_12 = native_hex(
    "488D9E00020000");
constexpr std::array<std::byte, 7> bytes_13 = native_hex(
    "4D8DB7E8010000");
constexpr std::array<std::byte, 7> bytes_14 = native_hex(
    "4D8DB700020000");
constexpr std::array<std::byte, 3> bytes_15 = native_hex(
    "83FE06");
constexpr std::array<std::byte, 3> bytes_16 = native_hex(
    "83FE09");
constexpr std::array<std::byte, 7> bytes_17 = native_hex(
    "498DBFE8010000");
constexpr std::array<std::byte, 7> bytes_18 = native_hex(
    "498DBF00020000");
constexpr std::array<std::byte, 3> bytes_19 = native_hex(
    "8D5606");
constexpr std::array<std::byte, 3> bytes_20 = native_hex(
    "8D5609");
constexpr std::array<std::byte, 5> bytes_21 = native_hex(
    "E89AAF1000");
constexpr std::array<std::byte, 7> bytes_22 = native_hex(
    "488BC34883C420");
constexpr std::array<ViewerBytes, 3> pieces = {{
    {0x1000, bytes_0},
    {0x2000, bytes_1},
    {0x2100, bytes_2},
}};
constexpr std::array<ViewerBytes, 0> guards = {{
}};
constexpr std::array<ViewerCopy, 2> copies = {{
    {0x100, 0x2714688, 0x8},
    {0x110, 0x2714698, 0x560},
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 7> relocations = {{
    {0x108, 0x2100, ViewerAddress::Cave, true},
    {0x10CF, 0x1EE1890, ViewerAddress::Module, false},
    {0x2024, 0x108, ViewerAddress::Cave, false},
    {0x202F, 0x0, ViewerAddress::Cave, false},
    {0x203B, 0x1DD6E7C, ViewerAddress::Module, false},
    {0x2104, 0x0, ViewerAddress::Cave, false},
    {0x2109, 0x751F20, ViewerAddress::Module, false},
}};
constexpr std::array<ViewerPatch, 13> patches = {{
    {0x1DB76B6, ViewerPatchKind::Bytes, bytes_3, bytes_4, 0x0, ViewerAddress::Cave},
    {0x1DD2405, ViewerPatchKind::Bytes, bytes_3, bytes_4, 0x0, ViewerAddress::Cave},
    {0x751F7A, ViewerPatchKind::Bytes, bytes_3, bytes_4, 0x0, ViewerAddress::Cave},
    {0x1DD5936, ViewerPatchKind::Bytes, bytes_5, bytes_6, 0x0, ViewerAddress::Cave},
    {0x1DD5B97, ViewerPatchKind::Bytes, bytes_7, bytes_8, 0x0, ViewerAddress::Cave},
    {0x1DD5A39, ViewerPatchKind::Bytes, bytes_9, bytes_10, 0x0, ViewerAddress::Cave},
    {0x1DD5A5D, ViewerPatchKind::Bytes, bytes_11, bytes_12, 0x0, ViewerAddress::Cave},
    {0x1DD5BC6, ViewerPatchKind::Bytes, bytes_13, bytes_14, 0x0, ViewerAddress::Cave},
    {0x1DD6A39, ViewerPatchKind::Bytes, bytes_15, bytes_16, 0x0, ViewerAddress::Cave},
    {0x1DD6A45, ViewerPatchKind::Bytes, bytes_17, bytes_18, 0x0, ViewerAddress::Cave},
    {0x1DD6B19, ViewerPatchKind::Bytes, bytes_19, bytes_20, 0x0, ViewerAddress::Cave},
    {0x1DD68F1, ViewerPatchKind::Call, bytes_21, {}, 0x1000, ViewerAddress::Cave},
    {0x1DD6E75, ViewerPatchKind::Jump, bytes_22, {}, 0x2000, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x1DD68F1, 0x10000, true,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace battle_map_data

namespace battle_info_data {
constexpr std::array<std::byte, 184> bytes_0 = native_hex(
    "9C50515253555657415041514152415341544155415641574881EC80000000F30F7F442420F30F7F4C2430F30F7F542440F30F7F5C2450F3"
    "0F7F642460F30F7F6C2470488D05000000004939067402EB144C89F1488B55E74C8B45EF4C8B4DF7E89B030000F30F6F442420F30F6F4C24"
    "30F30F6F542440F30F6F5C2450F30F6F642460F30F6F6C24704881C480000000415F415E415D415C415B415A415941585F5E5D5B5A59589D"
    "498B7608488D0500000000E900000000");
constexpr std::array<std::byte, 658> bytes_1 = native_hex(
    "5355565741544155415641574881EC880000004989CF48895424604C894424684C894C2470F048FF05DBEBFFFF4183BF500100002B0F853B"
    "0200004C8D35BEECFFFF31DBB9100000004D393E743449833E0075084885DB75034C89F34983C640FFC975E54885DB0F84090200004989DE"
    "4D893E498B87D8010000488B4020498946204531ED4B837CEE08000F85B8000000E800000000488B8818010000488B01BAB801000048C744"
    "24403500000048C7442448000000004C8D442440FF50284885C00F84AE0100004889C34889C1E800000000488D052EF3FFFF4889034B895C"
    "EE08F048FF0516EBFFFF4889D9BA05000000F3450F2C46204181C03C010000F3450F2C4E244C897C242048C74424280000000048C7442430"
    "01000000E800000000C7437004000000498D8F40010000418D55284989D841B914000000C744242001000000E80000000041FFC54183FD03"
    "0F822FFFFFFF4531ED4531E44183FC027412438D442D004401E0498B9CC7D8010000EB054B8B5CEE084885DB0F84F4000000488D0577F5FF"
    "FFF3420F1004A0F3410F584620F30F11442450488D056EF5FFFFF3420F1004A8F3410F584624F30F114424544889D9488D542450488B03FF"
    "909800000031F64A8B7CEC604885FF742A488B074889F9FF504884C0741D4A8BBCE7880000004885FF7410488B074889F9FF504884C07403"
    "4889FE4889D94889F24531C0E8000000004885F67434488B05000000004885C0740B0FB680880100003CFF7505E8000000004531C03C0174"
    "0641B8010000004889D94889F2E80000000031D24885F67405BA010000004889D9488B03FF900801000041FFC44183FC030F82F5FEFFFF41"
    "FFC54183FD030F82E5FEFFFFEB08F048FF0592E9FFFF4881C488000000415F415E415D415C5F5E5D5BC3");
constexpr std::array<std::byte, 116> bytes_2 = native_hex(
    "9C50415041514C8D05F3DCFFFF41B91000000049394808741749394810741B49394818741F4983C04041FFC975E5EB3949C7400800000000"
    "EB1249C7401000000000EB0849C7401800000000F048FF0DACDBFFFF498B4008490B4010490B4018750749C7000000000041594158589DE9"
    "00000000");
constexpr std::array<std::byte, 125> bytes_3 = native_hex(
    "5356574881EC00080000488904244881EC400800004889CB4D89CA4989D34889D6488D7C2420B9E0010000FCF348A5498DB340020000B90C"
    "000000F348A5498DB340020000B90C000000F348A5498DB340020000B90C000000F348A54889D9488D54242041B82B0000004D89D1E80000"
    "00004881C4401000005F5E5BC3");
constexpr std::array<std::byte, 45> bytes_4 = native_hex(
    "534883EC304889CBE80000000048894424204889D931D24531C04531C9E8DEDFFFFF488B4424204883C4305BC3");
constexpr std::array<std::byte, 82> bytes_5 = native_hex(
    "5356574883EC304889CBE8000000004889442420488D35E5CAFFFFBF100000004C3936740A4883C640FFCF75F3EB1631FF4889D9488B54FE"
    "08E800000000FFC783FF0372EC488B4424204883C4305F5E5BC3");
constexpr std::array<std::byte, 12> bytes_6 = native_hex(
    "0000000000001E4300009E43");
constexpr std::array<std::byte, 12> bytes_7 = native_hex(
    "000000C10000F04100008842");
constexpr std::array<std::byte, 5> bytes_8 = native_hex(
    "E88C110000");
constexpr std::array<std::byte, 5> bytes_9 = native_hex(
    "E838310600");
constexpr std::array<std::byte, 5> bytes_10 = native_hex(
    "E8D3C6FFFF");
constexpr std::array<std::byte, 11> bytes_11 = native_hex(
    "498B7608488D052F8B8D00");
constexpr std::array<std::byte, 9> bytes_12 = native_hex(
    "40534883EC20488BD9");
constexpr std::array<std::byte, 7> bytes_13 = native_hex(
    "4883EC484C8BD9");
constexpr std::array<ViewerBytes, 8> pieces = {{
    {0x1000, bytes_0},
    {0x1400, bytes_1},
    {0x2400, bytes_2},
    {0x3000, bytes_3},
    {0x3400, bytes_4},
    {0x3600, bytes_5},
    {0xB00, bytes_6},
    {0xB10, bytes_7},
}};
constexpr std::array<ViewerBytes, 6> guards = {{
    {0x1CADF2F, bytes_8},
    {0x1CB0D23, bytes_9},
    {0x1CB12C8, bytes_10},
    {0x1CADEBE, bytes_11},
    {0x162ECA0, bytes_12},
    {0x161AF00, bytes_13},
}};
constexpr std::array<ViewerCopy, 2> copies = {{
    {0x800, 0x25EE268, 0x8},
    {0x810, 0x25EE278, 0x260},
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 17> relocations = {{
    {0x1046, 0x26F7738, ViewerAddress::Module, false},
    {0x10AF, 0x25869F8, ViewerAddress::Module, false},
    {0x10B4, 0x1CADEC9, ViewerAddress::Module, false},
    {0x1492, 0x14ED0, ViewerAddress::Module, false},
    {0x14CF, 0x162ECA0, ViewerAddress::Module, false},
    {0x151D, 0x161AF00, ViewerAddress::Module, false},
    {0x1545, 0x1D16AA0, ViewerAddress::Module, false},
    {0x1605, 0x162EDA0, ViewerAddress::Module, false},
    {0x1611, 0x2E99C98, ViewerAddress::Module, false},
    {0x1626, 0x16AC280, ViewerAddress::Module, false},
    {0x163E, 0x162EEC0, ViewerAddress::Module, false},
    {0x2470, 0x1725E0, ViewerAddress::Module, false},
    {0x306E, 0x1D13E60, ViewerAddress::Module, false},
    {0x3409, 0x1CAD9A0, ViewerAddress::Module, false},
    {0x360B, 0x1CAF0C0, ViewerAddress::Module, false},
    {0x363A, 0x1CAF1F0, ViewerAddress::Module, false},
    {0x808, 0x2400, ViewerAddress::Cave, true},
}};
constexpr std::array<ViewerPatch, 4> patches = {{
    {0x1CADEBE, ViewerPatchKind::Jump, bytes_11, {}, 0x1000, ViewerAddress::Cave},
    {0x1CB12C8, ViewerPatchKind::Call, bytes_10, {}, 0x3400, ViewerAddress::Cave},
    {0x1CB0D23, ViewerPatchKind::Call, bytes_9, {}, 0x3000, ViewerAddress::Cave},
    {0x1CADF2F, ViewerPatchKind::Call, bytes_8, {}, 0x3600, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x1CADEBE, 0x10000, true,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace battle_info_data

namespace officer_info_data {
constexpr std::array<std::byte, 41> bytes_0 = native_hex(
    "41534C8D1DFFF0FFFF4C8918415B48C780D801000000000000F048FF05DFEFFFFF4889C6E900000000");
constexpr std::array<std::byte, 194> bytes_1 = native_hex(
    "488D0501F0FFFF4939017405E9000000004881EC0804000048898C24E00300004C898C24E80300004989D24C8D5C2420B96C000000498B02"
    "4989034983C2084983C308FFC975EE4C8D9424200300004C8D9C2480030000B90C000000498B024989034983C2084983C308FFC975EEC784"
    "2484030000E4040000C78424880300005C030000C784248C0300008C000000C784249003000024000000488B8C24E00300004C8B8C24E803"
    "0000488D54242041B80A000000E8000000004881C408040000C3");
constexpr std::array<std::byte, 51> bytes_2 = native_hex(
    "4C8D642450488DB5D402000050488D05F4ECFFFF48390758751441BD030000004C8D2529F0FFFF488D35D6EFFFFFE900000000");
constexpr std::array<std::byte, 45> bytes_3 = native_hex(
    "50488D0500ECFFFF4839450058740F4183FD020F8200000000E9000000004183FD030F8200000000E900000000");
constexpr std::array<std::byte, 45> bytes_4 = native_hex(
    "50488D0500EBFFFF4839450058740F4183FE020F8300000000E9000000004183FE030F8300000000E900000000");
constexpr std::array<std::byte, 30> bytes_5 = native_hex(
    "BE0200000050488D05FBE9FFFF48394500587505BE03000000E900000000");
constexpr std::array<std::byte, 15> bytes_6 = native_hex(
    "9CF048FF0DF7E7FFFF9DE900000000");
constexpr std::array<std::byte, 16> bytes_7 = native_hex(
    "E40400005C0300008C00000024000000");
constexpr std::array<std::byte, 12> bytes_8 = native_hex(
    "060000000800000009000000");
constexpr std::array<std::byte, 14> bytes_9 = native_hex(
    "40534883EC2033D241B8A0010000");
constexpr std::array<std::byte, 6> bytes_10 = native_hex(
    "41B809000000");
constexpr std::array<std::byte, 6> bytes_11 = native_hex(
    "41BD02000000");
constexpr std::array<std::byte, 5> bytes_12 = native_hex(
    "BAD8010000");
constexpr std::array<std::byte, 5> bytes_13 = native_hex(
    "488BF0EB03");
constexpr std::array<std::byte, 5> bytes_14 = native_hex(
    "E8D7100500");
constexpr std::array<std::byte, 12> bytes_15 = native_hex(
    "4C8D642450488DB5D4020000");
constexpr std::array<std::byte, 10> bytes_16 = native_hex(
    "4183FD020F8203FEFFFF");
constexpr std::array<std::byte, 6> bytes_17 = native_hex(
    "4183FE027341");
constexpr std::array<std::byte, 5> bytes_18 = native_hex(
    "BE02000000");
constexpr std::array<std::byte, 5> bytes_19 = native_hex(
    "BAE0010000");
constexpr std::array<ViewerBytes, 9> pieces = {{
    {0x1000, bytes_0},
    {0x1100, bytes_1},
    {0x1400, bytes_2},
    {0x1500, bytes_3},
    {0x1600, bytes_4},
    {0x1700, bytes_5},
    {0x1800, bytes_6},
    {0x420, bytes_7},
    {0x450, bytes_8},
}};
constexpr std::array<ViewerBytes, 10> guards = {{
    {0x1CFBCF0, bytes_9},
    {0x1CC2D76, bytes_10},
    {0x1CC2FC6, bytes_11},
    {0x1D00092, bytes_12},
    {0x1D000AC, bytes_13},
    {0x1CC2D84, bytes_14},
    {0x1CC3003, bytes_15},
    {0x1CC2563, bytes_16},
    {0x1CC256D, bytes_17},
    {0x1CC2581, bytes_18},
}};
constexpr std::array<ViewerCopy, 4> copies = {{
    {0x100, 0x26F92C0, 0x8},
    {0x110, 0x26F92D0, 0x240},
    {0x400, 0x2C13A90, 0x10},
    {0x410, 0x2C13AB0, 0x10},
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 15> relocations = {{
    {0x1025, 0x1D000B4, ViewerAddress::Module, false},
    {0x110D, 0x1D13E60, ViewerAddress::Module, false},
    {0x11B6, 0x1D13E60, ViewerAddress::Module, false},
    {0x142F, 0x1CC300F, ViewerAddress::Module, false},
    {0x1515, 0x1CC2370, ViewerAddress::Module, false},
    {0x151A, 0x1CC256D, ViewerAddress::Module, false},
    {0x1524, 0x1CC2370, ViewerAddress::Module, false},
    {0x1529, 0x1CC256D, ViewerAddress::Module, false},
    {0x1615, 0x1CC25B4, ViewerAddress::Module, false},
    {0x161A, 0x1CC2573, ViewerAddress::Module, false},
    {0x1624, 0x1CC25B4, ViewerAddress::Module, false},
    {0x1629, 0x1CC2573, ViewerAddress::Module, false},
    {0x171A, 0x1CC2586, ViewerAddress::Module, false},
    {0x180B, 0x6AFA70, ViewerAddress::Module, false},
    {0x108, 0x1800, ViewerAddress::Cave, true},
}};
constexpr std::array<ViewerPatch, 7> patches = {{
    {0x1D000AC, ViewerPatchKind::Jump, bytes_13, {}, 0x1000, ViewerAddress::Cave},
    {0x1CC2D84, ViewerPatchKind::Call, bytes_14, {}, 0x1100, ViewerAddress::Cave},
    {0x1CC3003, ViewerPatchKind::Jump, bytes_15, {}, 0x1400, ViewerAddress::Cave},
    {0x1CC2563, ViewerPatchKind::Jump, bytes_16, {}, 0x1500, ViewerAddress::Cave},
    {0x1CC256D, ViewerPatchKind::Jump, bytes_17, {}, 0x1600, ViewerAddress::Cave},
    {0x1CC2581, ViewerPatchKind::Jump, bytes_18, {}, 0x1700, ViewerAddress::Cave},
    {0x1D00092, ViewerPatchKind::Bytes, bytes_12, bytes_19, 0x0, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x1CC3003, 0x10000, true,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace officer_info_data

namespace kirase_data {
constexpr std::array<std::byte, 1256> bytes_0 = native_hex(
    "56574156B820270000E8000000004829C448C7442428FEFFFFFF48899C24402700004889AC2450270000488B05000000004831E048898424"
    "F02400004889D64885D20F8473040000488B024889D1FF504884C00F84620400004889B424E8200000488BBE880000004C8BB69000000048"
    "85FF0F8443040000488B074889F9FF504884C00F84320400004D85F60F8429040000498B064C89F1FF504884C00F8418040000488B8E9800"
    "00004885C90F8408040000488B01FF504884C00F84FA03000031D241B800020000488D8C24F0200000E80000000031D241B800020000488D"
    "8C24F0220000E800000000488D0D00000000E8000000004889F9E8000000004889C34889F1E8000000004889C5488D3D0000000031C0B900"
    "020000F3AA4885ED741FE800000000E8000000004989E941B8FE010000488D1500000000E800000000488D3D0000000031C0B900020000F3"
    "AA4885DB741FE800000000E8000000004989D941B8FE010000488D1500000000E800000000488D3D0000000031C0B900020000F3AA488D3D"
    "00000000B900020000F3AA488D3D00000000B900020000F3AABA495C0300488D0D00000000488B0500000000FF50084889C7E80000000031"
    "DB89D94889F84885FF750489D9EB1166391F740C90488D4002FFC166391875F548FFC14881F9FF0000007607B9FF000000EB054885C97422"
    "4C8D8424F02000004929F889DA0FB7076641890438488D7F02FFC289D04839C872EB4C89F1E8000000004889C54889F1E8000000004889C6"
    "488D3D0000000031C0B900020000F3AA4885F6741FE800000000E8000000004989F141B8FE010000488D1500000000E800000000488D3D00"
    "00000031C0B900020000F3AA4885ED741FE800000000E8000000004989E941B8FE010000488D1500000000E800000000488D3D0000000031"
    "C0B900020000F3AA488D3D00000000B900020000F3AA488D3D00000000B900020000F3AABA495C0300488D0D00000000488B0500000000FF"
    "50084889C241B8FF000000488D8C24F0220000E800000000488BB424E82000004C8BB69800000031D241B800020000488D8C2400250000E8"
    "000000004C89F1E8000000004889C54889F1E8000000004889C6488D3D0000000031C0B900020000F3AA4885F6741FE800000000E8000000"
    "004989F141B8FE010000488D1500000000E800000000488D3D0000000031C0B900020000F3AA4885ED741FE800000000E8000000004989E9"
    "41B8FE010000488D1500000000E800000000488D3D0000000031C0B900020000F3AA488D3D00000000B900020000F3AA488D3D00000000B9"
    "00020000F3AABA495C0300488D0D00000000488B0500000000FF50084889C241B8FF000000488D8C2400250000E800000000488D84240025"
    "00004889842410270000488D8424F02000004889842400270000488D8424F02200004889842408270000BA38030000488D0D00000000E800"
    "0000004889842418270000488D4C2430E80000000090C744243804000000BA510D0000488D0D00000000E8000000004889C7E80000000048"
    "89F94885FF741366833F00740D90488D4902FFC36683390075F489D841B8FF0F00004C39C0440F42C04889FA488D4C2448E800000000488D"
    "542430488D8C2400270000E80000000089C7488D8C2460200000E80000000089F8EB05B803000000488B8C24F02400004831E1E800000000"
    "4C8D9C2420270000498B5B20498B6B304C89DC415E5F5EC3");
constexpr std::array<std::byte, 5> bytes_1 = native_hex(
    "4056574156");
constexpr std::array<std::byte, 4> bytes_2 = native_hex(
    "4983FD02");
constexpr std::array<std::byte, 3> bytes_3 = native_hex(
    "83F801");
constexpr std::array<std::byte, 4> bytes_4 = native_hex(
    "4983FD03");
constexpr std::array<std::byte, 3> bytes_5 = native_hex(
    "83F802");
constexpr std::array<ViewerBytes, 1> pieces = {{
    {0x0, bytes_0},
}};
constexpr std::array<ViewerBytes, 4> guards = {{
    {0x1BC6B20, bytes_1},
    {0x1BC70AF, bytes_2},
    {0x1BC70C0, bytes_3},
    {0x1BC738C, bytes_2},
}};
constexpr std::array<ViewerCopy, 0> copies = {{
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 71> relocations = {{
    {0xA, 0x1229490, ViewerAddress::Module, false},
    {0x2D, 0x2C03448, ViewerAddress::Module, false},
    {0xD2, 0x124E4C0, ViewerAddress::Module, false},
    {0xE7, 0x124E4C0, ViewerAddress::Module, false},
    {0xEE, 0x2C64650, ViewerAddress::Module, false},
    {0xF3, 0x1720240, ViewerAddress::Module, false},
    {0xFB, 0x1B435F0, ViewerAddress::Module, false},
    {0x106, 0x1B45160, ViewerAddress::Module, false},
    {0x110, 0x2C65D98, ViewerAddress::Module, false},
    {0x123, 0x14C70, ViewerAddress::Module, false},
    {0x128, 0x14C70, ViewerAddress::Module, false},
    {0x138, 0x2C65D98, ViewerAddress::Module, false},
    {0x13D, 0x14510, ViewerAddress::Module, false},
    {0x144, 0x2C65F98, ViewerAddress::Module, false},
    {0x157, 0x14C70, ViewerAddress::Module, false},
    {0x15C, 0x14C70, ViewerAddress::Module, false},
    {0x16C, 0x2C65F98, ViewerAddress::Module, false},
    {0x171, 0x14510, ViewerAddress::Module, false},
    {0x178, 0x2C66198, ViewerAddress::Module, false},
    {0x188, 0x2C66398, ViewerAddress::Module, false},
    {0x196, 0x2C66598, ViewerAddress::Module, false},
    {0x1A9, 0x2C64650, ViewerAddress::Module, false},
    {0x1B0, 0x2C64650, ViewerAddress::Module, false},
    {0x1BB, 0x14C70, ViewerAddress::Module, false},
    {0x21E, 0x1B435F0, ViewerAddress::Module, false},
    {0x229, 0x1B45160, ViewerAddress::Module, false},
    {0x233, 0x2C65D98, ViewerAddress::Module, false},
    {0x246, 0x14C70, ViewerAddress::Module, false},
    {0x24B, 0x14C70, ViewerAddress::Module, false},
    {0x25B, 0x2C65D98, ViewerAddress::Module, false},
    {0x260, 0x14510, ViewerAddress::Module, false},
    {0x267, 0x2C65F98, ViewerAddress::Module, false},
    {0x27A, 0x14C70, ViewerAddress::Module, false},
    {0x27F, 0x14C70, ViewerAddress::Module, false},
    {0x28F, 0x2C65F98, ViewerAddress::Module, false},
    {0x294, 0x14510, ViewerAddress::Module, false},
    {0x29B, 0x2C66198, ViewerAddress::Module, false},
    {0x2AB, 0x2C66398, ViewerAddress::Module, false},
    {0x2B9, 0x2C66598, ViewerAddress::Module, false},
    {0x2CC, 0x2C64650, ViewerAddress::Module, false},
    {0x2D3, 0x2C64650, ViewerAddress::Module, false},
    {0x2EC, 0x17204C0, ViewerAddress::Module, false},
    {0x310, 0x124E4C0, ViewerAddress::Module, false},
    {0x318, 0x1B435F0, ViewerAddress::Module, false},
    {0x323, 0x1B45160, ViewerAddress::Module, false},
    {0x32D, 0x2C65D98, ViewerAddress::Module, false},
    {0x340, 0x14C70, ViewerAddress::Module, false},
    {0x345, 0x14C70, ViewerAddress::Module, false},
    {0x355, 0x2C65D98, ViewerAddress::Module, false},
    {0x35A, 0x14510, ViewerAddress::Module, false},
    {0x361, 0x2C65F98, ViewerAddress::Module, false},
    {0x374, 0x14C70, ViewerAddress::Module, false},
    {0x379, 0x14C70, ViewerAddress::Module, false},
    {0x389, 0x2C65F98, ViewerAddress::Module, false},
    {0x38E, 0x14510, ViewerAddress::Module, false},
    {0x395, 0x2C66198, ViewerAddress::Module, false},
    {0x3A5, 0x2C66398, ViewerAddress::Module, false},
    {0x3B3, 0x2C66598, ViewerAddress::Module, false},
    {0x3C6, 0x2C64650, ViewerAddress::Module, false},
    {0x3CD, 0x2C64650, ViewerAddress::Module, false},
    {0x3E6, 0x17204C0, ViewerAddress::Module, false},
    {0x422, 0x2C64398, ViewerAddress::Module, false},
    {0x427, 0x17DAF60, ViewerAddress::Module, false},
    {0x439, 0x583360, ViewerAddress::Module, false},
    {0x44E, 0x2C64398, ViewerAddress::Module, false},
    {0x453, 0x17DAF60, ViewerAddress::Module, false},
    {0x45B, 0x14C70, ViewerAddress::Module, false},
    {0x492, 0x17204C0, ViewerAddress::Module, false},
    {0x4A4, 0x596DB0, ViewerAddress::Module, false},
    {0x4B3, 0x583420, ViewerAddress::Module, false},
    {0x4CC, 0x12286E0, ViewerAddress::Module, false},
}};
constexpr std::array<ViewerPatch, 4> patches = {{
    {0x1BC6B20, ViewerPatchKind::Jump, bytes_1, {}, 0x0, ViewerAddress::Cave},
    {0x1BC70AF, ViewerPatchKind::Bytes, bytes_2, bytes_4, 0x0, ViewerAddress::Cave},
    {0x1BC70C0, ViewerPatchKind::Bytes, bytes_3, bytes_5, 0x0, ViewerAddress::Cave},
    {0x1BC738C, ViewerPatchKind::Bytes, bytes_2, bytes_4, 0x0, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x1BC6B20, 0x2000, false,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace kirase_data

namespace in_progress_data {
constexpr std::array<std::byte, 941> bytes_0 = native_hex(
    "4889909000000048899098000000E9000000004889F9488D95E00B0000448B85E00B0000442B85D00B00004589E9E83D000000488B050000"
    "0000E9000000004883EC3848894C2420E8190000004889442428488B4C2420E838010000488B4424284883C438C34053554155E900000000"
    "5356575541544883EC604889CB4889D64489C54589CC48C7836002000000000000E800000000488B8818010000C74424403500000048C744"
    "2448000000004C8D442440BAB8010000488B01FF50284885C00F84C20000004889C74889C1E800000000488D0500000000488907C787A001"
    "00000000000066C787A40100000000C787A8010000FFFFFFFF48C787B0010000000000004889BB600200004889F94489E2448B064101E844"
    "8B4E048B46088944242048895C2428C644243001C644243800488B07FF9040020000E800000000488B4828C74424403700000048C7442448"
    "000000004C8D442440BA10000000488B01FF50284885C0742048C70000000000488978084889C2488D8B4001000041B801000000E8000000"
    "004883C460415C5D5F5E5BC3405355415541564883EC2848897424584C8BE948897C2460BD020000004C897C242041BE98000000498B9D60"
    "0200004885DB0F843601000083FD030F828000000033FF33D24032F6488B03488BCBFF90080100004889BBB00100004084F67458488BCFE8"
    "00000000488B8B48010000488BF04C8B7910E8000000004533C0488BCE4885F67419664439067413660F1F440000488D490241FFC0668339"
    "0075F3498B07488BD6498BCFFF90C00100000FB747088983A801000083FD03723633DBEB3D498B85A8010000498B3C064885FF0F846EFFFF"
    "FF488B07488BCFFF504884C00F845DFFFFFF40B601BA01000000E955FFFFFF498B85A8010000498B1C06498BB5600200004885DB7464488B"
    "03488BCBFF504884C074570FB64B2E33FF83E901742283E901741183F901752ABA3C030000BF90000000EB16BA3B030000BF8F000000EB0A"
    "BA3A030000BF85000000488BCEE80000000085FF7414488D8E40010000448BC7BA02000000E800000000FFC54983C60883FD030F8CABFEFF"
    "FF4C8B7C2420488B7C2460488B7424584883C428415E415D5D5BC3CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC498B803802000049"
    "8B88A801000048898198000000C78174030000FFFFFFFFE900000000CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
    "488B8290000000483981300200000F8500000000488B8298000000483981380200000F8500000000E900000000");
constexpr std::array<std::byte, 7> bytes_1 = native_hex(
    "48899090000000");
constexpr std::array<std::byte, 5> bytes_2 = native_hex(
    "BA60020000");
constexpr std::array<std::byte, 7> bytes_3 = native_hex(
    "488B054900BE01");
constexpr std::array<std::byte, 5> bytes_4 = native_hex(
    "4053554155");
constexpr std::array<std::byte, 3> bytes_5 = native_hex(
    "83FF02");
constexpr std::array<std::byte, 7> bytes_6 = native_hex(
    "C7458402000000");
constexpr std::array<std::byte, 3> bytes_7 = native_hex(
    "83FA02");
constexpr std::array<std::byte, 7> bytes_8 = native_hex(
    "498B88A8010000");
constexpr std::array<std::byte, 7> bytes_9 = native_hex(
    "488B8290000000");
constexpr std::array<std::byte, 6> bytes_10 = native_hex(
    "0F863B010000");
constexpr std::array<std::byte, 5> bytes_11 = native_hex(
    "BA70020000");
constexpr std::array<std::byte, 1> bytes_12 = native_hex(
    "02");
constexpr std::array<std::byte, 1> bytes_13 = native_hex(
    "03");
constexpr std::array<std::byte, 6> bytes_14 = native_hex(
    "E93C01000090");
constexpr std::array<ViewerBytes, 1> pieces = {{
    {0x0, bytes_0},
}};
constexpr std::array<ViewerBytes, 12> guards = {{
    {0x12B67EE, bytes_1},
    {0x12B5F7A, bytes_2},
    {0x12B9C48, bytes_3},
    {0x12B7030, bytes_4},
    {0x12E273A, bytes_5},
    {0x12E280E, bytes_6},
    {0x12E2A33, bytes_7},
    {0x12E2A41, bytes_7},
    {0x12E2A6D, bytes_7},
    {0x12B4E1C, bytes_8},
    {0x12B5092, bytes_9},
    {0x12E2899, bytes_10},
}};
constexpr std::array<ViewerCopy, 0> copies = {{
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 17> relocations = {{
    {0x36, 0x2E99C98, ViewerAddress::Module, false},
    {0x92, 0x14ED0, ViewerAddress::Module, false},
    {0xCE, 0x1EDE250, ViewerAddress::Module, false},
    {0xD5, 0x25EE270, ViewerAddress::Module, false},
    {0x13B, 0x14ED0, ViewerAddress::Module, false},
    {0x185, 0x1D0D250, ViewerAddress::Module, false},
    {0x1F8, 0x170E2D0, ViewerAddress::Module, false},
    {0x20B, 0x14C70, ViewerAddress::Module, false},
    {0x2E6, 0x162C650, ViewerAddress::Module, false},
    {0x2FE, 0x1D17280, ViewerAddress::Module, false},
    {0xF, 0x12B67F5, ViewerAddress::Module, false},
    {0x3B, 0x12B9C4F, ViewerAddress::Module, false},
    {0x6C, 0x12B7035, ViewerAddress::Module, false},
    {0x360, 0x12B4E23, ViewerAddress::Module, false},
    {0x390, 0x12B517E, ViewerAddress::Module, false},
    {0x3A4, 0x12B517E, ViewerAddress::Module, false},
    {0x3A9, 0x12B50A6, ViewerAddress::Module, false},
}};
constexpr std::array<ViewerPatch, 12> patches = {{
    {0x12B5F7A, ViewerPatchKind::Bytes, bytes_2, bytes_11, 0x0, ViewerAddress::Cave},
    {0x12B9C48, ViewerPatchKind::Jump, bytes_3, {}, 0x13, ViewerAddress::Cave},
    {0x12B7030, ViewerPatchKind::Jump, bytes_4, {}, 0x3F, ViewerAddress::Cave},
    {0x12B67EE, ViewerPatchKind::Jump, bytes_1, {}, 0x0, ViewerAddress::Cave},
    {0x12E273C, ViewerPatchKind::Bytes, bytes_12, bytes_13, 0x0, ViewerAddress::Cave},
    {0x12E2811, ViewerPatchKind::Bytes, bytes_12, bytes_13, 0x0, ViewerAddress::Cave},
    {0x12E2A35, ViewerPatchKind::Bytes, bytes_12, bytes_13, 0x0, ViewerAddress::Cave},
    {0x12E2A43, ViewerPatchKind::Bytes, bytes_12, bytes_13, 0x0, ViewerAddress::Cave},
    {0x12E2A6F, ViewerPatchKind::Bytes, bytes_12, bytes_13, 0x0, ViewerAddress::Cave},
    {0x12B4E1C, ViewerPatchKind::Jump, bytes_8, {}, 0x340, ViewerAddress::Cave},
    {0x12B5092, ViewerPatchKind::Jump, bytes_9, {}, 0x380, ViewerAddress::Cave},
    {0x12E2899, ViewerPatchKind::Bytes, bytes_10, bytes_14, 0x0, ViewerAddress::Cave},
}};
constexpr ViewerSpec spec{0x12B67EE, 0x8192, false,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace in_progress_data

namespace base_editor_data {
constexpr std::array<std::byte, 1387> bytes_0 = native_hex(
    "4889997802000048899980020000E9000000004889997802000048899980020000E9000000004889BE780200004889BE80020000E9000000"
    "004889817802000048898180020000E9000000004889867802000048898680020000E9000000004889867802000048898680020000E90000"
    "0000405355565741564883EC40488B81A001000048894C2438488B10488B6A08837D00010F858A000000488B6D084885ED0F84240300004C"
    "8DB17002000033FF8BDF4C89742430498BF6488B0E4885C9743E488B01FF504884C0743485DB784483FB03773F4863C3488D0C4500000000"
    "488B064885C0750A6689BC292E010000EB220FB74008668984292E010000EB1485DB781083FB03770B4863C36689BC452E010000FFC34883"
    "C60883FB03729BE985000000488B6D104885ED0F849A0200004C8DB17002000033FF8BDF4C89742430498BF6488B0E4885C9743E488B01FF"
    "504884C0743485DB784483FB03773F4863C3488D0C4500000000488B064885C0750A6689BC292E010000EB220FB74008668984292E010000"
    "EB1485DB781083FB03770B4863C36689BC452E010000FFC34883C60883FB03729B4C8964247841BC030000004C89AC2480000000448BEF4C"
    "89BC24880000004D8BFE458BF44C89642428C7442420010000000F1F84000000000049837F18000F842F010000498B0F4885C90F84230100"
    "00488B01FF504884C00F8415010000498B1F498B6F184885DB7417488B03488BCBFF504884C0740A40B601BA01000000EB058BD74032F648"
    "8B4500488BCDFF900801000048899DB00100004084F67461488BCBE800000000488B8D48010000488BF04C8B6110E800000000448BC7488B"
    "CE4885F6741C66833E007416660F1F840000000000488D490241FFC06683390075F3498B0424488BD6498BCCFF90C00100000FB743084C8B"
    "6424288985A8010000498B37498B6F184885F67464488B06488BCEFF504884C074570FB64E2E8BDF83E901742283E901741183F901752ABA"
    "40030000BB90000000EB16BA3F030000BB8F000000EB0ABA3E030000BB85000000488BCDE80000000085DB7414488D8D40010000448BC3BA"
    "02000000E80000000041FFC54983C7084983EC014C896424280F85B3FEFFFF4C8B642478453BEE4C8B7424304C8B7C24387343418BF5BD03"
    "0000004883C651412BED498D34F7660F1F840000000000488B1E4885DB741533D2488BCB488B03FF90080100004889BBB0010000488D7608"
    "4883ED0175D9498B8F680200004C8BBC24880000004C8BAC24800000004885C97429498D46184C3BF0740F49833E00750D4983C6084C3BF0"
    "75F1897C2420488B018B542420FF90100100004883C440415E5F5E5D5BC34889F1E80A000000E800000000E900000000534883EC204889CB"
    "488B83B001000048898388020000488B83B801000048898390020000E800000000488D150000000041B8960000003C01750D488D15000000"
    "0041B8E40000004889D941B901000000E8060000004883C4205BC35356575541544883EC604889CB4889D64489C54589CC48C78398020000"
    "00000000E800000000488B8818010000C74424403500000048C7442448000000004C8D442440BAB8010000488B01FF50284885C00F84C600"
    "00004889C74889C1E800000000488D0500000000488907C787A00100000000000066C787A40100000000C787A8010000FFFFFFFF48C787B0"
    "010000000000004889BB98020000834F40084889F94489E2448B064101E8448B4E048B46088944242048895C2428C644243001C644243801"
    "488B07FF9040020000E800000000488B4828C74424403700000048C7442448000000004C8D442440BA10000000488B01FF50284885C07420"
    "48C70000000000488978084889C2488D8B4001000041B801000000E8000000004883C460415C5D5F5E5BC3");
constexpr std::array<std::byte, 7> bytes_1 = native_hex(
    "488981F8010000");
constexpr std::array<std::byte, 5> bytes_2 = native_hex(
    "BA70020000");
constexpr std::array<std::byte, 7> bytes_3 = native_hex(
    "488999F8010000");
constexpr std::array<std::byte, 8> bytes_4 = native_hex(
    "4889B4C5F8010000");
constexpr std::array<std::byte, 7> bytes_5 = native_hex(
    "488D8EF8010000");
constexpr std::array<std::byte, 7> bytes_6 = native_hex(
    "4889BEF8010000");
constexpr std::array<std::byte, 8> bytes_7 = native_hex(
    "4C89BCC6F8010000");
constexpr std::array<std::byte, 7> bytes_8 = native_hex(
    "488D81F8010000");
constexpr std::array<std::byte, 7> bytes_9 = native_hex(
    "488986F8010000");
constexpr std::array<std::byte, 7> bytes_10 = native_hex(
    "48899900020000");
constexpr std::array<std::byte, 7> bytes_11 = native_hex(
    "4889BE00020000");
constexpr std::array<std::byte, 7> bytes_12 = native_hex(
    "48898100020000");
constexpr std::array<std::byte, 7> bytes_13 = native_hex(
    "48898600020000");
constexpr std::array<std::byte, 3> bytes_14 = native_hex(
    "83FF02");
constexpr std::array<std::byte, 6> bytes_15 = native_hex(
    "41BD02000000");
constexpr std::array<std::byte, 7> bytes_16 = native_hex(
    "C745B402000000");
constexpr std::array<std::byte, 4> bytes_17 = native_hex(
    "4183FC02");
constexpr std::array<std::byte, 4> bytes_18 = native_hex(
    "488D4110");
constexpr std::array<std::byte, 7> bytes_19 = native_hex(
    "4881C108020000");
constexpr std::array<std::byte, 7> bytes_20 = native_hex(
    "40535556574156");
constexpr std::array<std::byte, 5> bytes_21 = native_hex(
    "E8B274B2FE");
constexpr std::array<std::byte, 5> bytes_22 = native_hex(
    "BAA0020000");
constexpr std::array<std::byte, 7> bytes_23 = native_hex(
    "48899970020000");
constexpr std::array<std::byte, 8> bytes_24 = native_hex(
    "4889B4C570020000");
constexpr std::array<std::byte, 7> bytes_25 = native_hex(
    "488D8E70020000");
constexpr std::array<std::byte, 7> bytes_26 = native_hex(
    "4889BE70020000");
constexpr std::array<std::byte, 8> bytes_27 = native_hex(
    "4C89BCC670020000");
constexpr std::array<std::byte, 7> bytes_28 = native_hex(
    "488D8170020000");
constexpr std::array<std::byte, 7> bytes_29 = native_hex(
    "48898170020000");
constexpr std::array<std::byte, 7> bytes_30 = native_hex(
    "48898670020000");
constexpr std::array<std::byte, 3> bytes_31 = native_hex(
    "83FF03");
constexpr std::array<std::byte, 6> bytes_32 = native_hex(
    "41BD03000000");
constexpr std::array<std::byte, 7> bytes_33 = native_hex(
    "C745B403000000");
constexpr std::array<std::byte, 4> bytes_34 = native_hex(
    "4183FC03");
constexpr std::array<std::byte, 4> bytes_35 = native_hex(
    "488D4118");
constexpr std::array<std::byte, 7> bytes_36 = native_hex(
    "4881C188020000");
constexpr std::array<ViewerBytes, 1> pieces = {{
    {0x0, bytes_0},
}};
constexpr std::array<ViewerBytes, 31> guards = {{
    {0x14629C, bytes_1},
    {0x14F4235, bytes_2},
    {0x130BCA, bytes_2},
    {0x14E9D71, bytes_3},
    {0x14E9DD7, bytes_4},
    {0x14E9ECB, bytes_3},
    {0x14E9F33, bytes_4},
    {0x14EA6B4, bytes_5},
    {0x14EA835, bytes_6},
    {0x14EA89E, bytes_7},
    {0x14EB525, bytes_8},
    {0x14EB5E7, bytes_1},
    {0x14EEB06, bytes_9},
    {0x14F42BF, bytes_9},
    {0x14E9D78, bytes_10},
    {0x14E9ED2, bytes_10},
    {0x14EA83C, bytes_11},
    {0x14EB5EE, bytes_12},
    {0x14EEB0D, bytes_13},
    {0x14F42C6, bytes_13},
    {0x14E9DE1, bytes_14},
    {0x14E9F3D, bytes_14},
    {0x14EA6C5, bytes_15},
    {0x14EA7CA, bytes_16},
    {0x14EA895, bytes_17},
    {0x14EA6BE, bytes_18},
    {0x14EB52C, bytes_19},
    {0x14EB850, bytes_8},
    {0x14EB857, bytes_19},
    {0x14E97F0, bytes_20},
    {0x14EDA19, bytes_21},
}};
constexpr std::array<ViewerCopy, 0> copies = {{
}};
constexpr std::array<ViewerPointerGuard, 0> pointer_guards = {{
}};
constexpr std::array<ViewerRelocation, 20> relocations = {{
    {0x24C, 0x170E2D0, ViewerAddress::Module, false},
    {0x25F, 0x14C70, ViewerAddress::Module, false},
    {0x2FD, 0x162C650, ViewerAddress::Module, false},
    {0x315, 0x1D17280, ViewerAddress::Module, false},
    {0x3DF, 0x14ED0, ViewerAddress::Module, false},
    {0x40D, 0x16AC280, ViewerAddress::Module, false},
    {0x414, 0x2C20D40, ViewerAddress::Module, false},
    {0x425, 0x2C20F90, ViewerAddress::Module, false},
    {0x465, 0x14ED0, ViewerAddress::Module, false},
    {0x4A1, 0x1EDE250, ViewerAddress::Module, false},
    {0x4A8, 0x25EE270, ViewerAddress::Module, false},
    {0x512, 0x14ED0, ViewerAddress::Module, false},
    {0x55C, 0x1D0D250, ViewerAddress::Module, false},
    {0xF, 0x14E9D7F, ViewerAddress::Module, false},
    {0x22, 0x14E9ED9, ViewerAddress::Module, false},
    {0x35, 0x14EA843, ViewerAddress::Module, false},
    {0x48, 0x14EB5F5, ViewerAddress::Module, false},
    {0x5B, 0x14EEB14, ViewerAddress::Module, false},
    {0x6E, 0x14F42CD, ViewerAddress::Module, false},
    {0x3E4, 0x14EDA1E, ViewerAddress::Module, false},
}};
constexpr std::array<ViewerPatch, 31> patches = {{
    {0x14F4235, ViewerPatchKind::Bytes, bytes_2, bytes_22, 0x0, ViewerAddress::Cave},
    {0x130BCA, ViewerPatchKind::Bytes, bytes_2, bytes_22, 0x0, ViewerAddress::Cave},
    {0x14E9D71, ViewerPatchKind::Bytes, bytes_3, bytes_23, 0x0, ViewerAddress::Cave},
    {0x14E9DD7, ViewerPatchKind::Bytes, bytes_4, bytes_24, 0x0, ViewerAddress::Cave},
    {0x14E9ECB, ViewerPatchKind::Bytes, bytes_3, bytes_23, 0x0, ViewerAddress::Cave},
    {0x14E9F33, ViewerPatchKind::Bytes, bytes_4, bytes_24, 0x0, ViewerAddress::Cave},
    {0x14EA6B4, ViewerPatchKind::Bytes, bytes_5, bytes_25, 0x0, ViewerAddress::Cave},
    {0x14EA835, ViewerPatchKind::Bytes, bytes_6, bytes_26, 0x0, ViewerAddress::Cave},
    {0x14EA89E, ViewerPatchKind::Bytes, bytes_7, bytes_27, 0x0, ViewerAddress::Cave},
    {0x14EB525, ViewerPatchKind::Bytes, bytes_8, bytes_28, 0x0, ViewerAddress::Cave},
    {0x14EB5E7, ViewerPatchKind::Bytes, bytes_1, bytes_29, 0x0, ViewerAddress::Cave},
    {0x14EEB06, ViewerPatchKind::Bytes, bytes_9, bytes_30, 0x0, ViewerAddress::Cave},
    {0x14F42BF, ViewerPatchKind::Bytes, bytes_9, bytes_30, 0x0, ViewerAddress::Cave},
    {0x14E9D78, ViewerPatchKind::Jump, bytes_10, {}, 0x0, ViewerAddress::Cave},
    {0x14E9ED2, ViewerPatchKind::Jump, bytes_10, {}, 0x13, ViewerAddress::Cave},
    {0x14EA83C, ViewerPatchKind::Jump, bytes_11, {}, 0x26, ViewerAddress::Cave},
    {0x14EB5EE, ViewerPatchKind::Jump, bytes_12, {}, 0x39, ViewerAddress::Cave},
    {0x14EEB0D, ViewerPatchKind::Jump, bytes_13, {}, 0x4C, ViewerAddress::Cave},
    {0x14F42C6, ViewerPatchKind::Jump, bytes_13, {}, 0x5F, ViewerAddress::Cave},
    {0x14E9DE1, ViewerPatchKind::Bytes, bytes_14, bytes_31, 0x0, ViewerAddress::Cave},
    {0x14E9F3D, ViewerPatchKind::Bytes, bytes_14, bytes_31, 0x0, ViewerAddress::Cave},
    {0x14EA6C5, ViewerPatchKind::Bytes, bytes_15, bytes_32, 0x0, ViewerAddress::Cave},
    {0x14EA7CA, ViewerPatchKind::Bytes, bytes_16, bytes_33, 0x0, ViewerAddress::Cave},
    {0x14EA895, ViewerPatchKind::Bytes, bytes_17, bytes_34, 0x0, ViewerAddress::Cave},
    {0x14EA6BE, ViewerPatchKind::Bytes, bytes_18, bytes_35, 0x0, ViewerAddress::Cave},
    {0x14EB52C, ViewerPatchKind::Bytes, bytes_19, bytes_36, 0x0, ViewerAddress::Cave},
    {0x14EB850, ViewerPatchKind::Bytes, bytes_8, bytes_28, 0x0, ViewerAddress::Cave},
    {0x14EB857, ViewerPatchKind::Bytes, bytes_19, bytes_36, 0x0, ViewerAddress::Cave},
    {0x14E97F0, ViewerPatchKind::Jump, bytes_20, {}, 0x72, ViewerAddress::Cave},
    {0x14EDA19, ViewerPatchKind::Jump, bytes_21, {}, 0x3D6, ViewerAddress::Cave},
    {0x14629C, ViewerPatchKind::Jump, bytes_1, {}, 0x14EB5E5, ViewerAddress::Module},
}};
constexpr ViewerSpec spec{0x14629C, 0x8192, false,
    pieces, guards, copies, pointer_guards, relocations, patches};
} // namespace base_editor_data

constexpr std::array<const ViewerSpec*, TraitViewerNative::GROUP_COUNT> VIEWER_SPECS{{
    &roster_data::spec,
    &widths_data::spec,
    &battle_prep_data::spec,
    &battle_map_data::spec,
    &battle_info_data::spec,
    &officer_info_data::spec,
    &kirase_data::spec,
    &in_progress_data::spec,
    &base_editor_data::spec,
}};
