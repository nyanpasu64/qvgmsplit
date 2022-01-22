This is an attempted port of qvgmsplit to meson.

- Meson's Qt support is half-baked; all the community effort is pouring into CMake.
- At first I got linking errors.
    - `project(vgm-player)` contains `target_link_libraries(${PROJECT_NAME} PRIVATE ${PLAYER_LIBS} vgm-emu vgm-utils)`, but `vgm-emu` isn't being linked.
    - `vgm-emu` is a private dependency of other libvgm CMake targets. If I omit it from qvgmsplit, both libvgm and qvgmsplit link fine in CMake (qvgmsplit arguably shouldn't), but both libvgm and qvgmsplit fail to link in Meson (libvgm arguably should):

```
[1/1] Linking target qvgmsplit
FAILED: qvgmsplit
c++  -o qvgmsplit qvgmsplit.p/meson-generated_moc_gui_app.cpp.o qvgmsplit.p/meson-generated_moc_mainwindow.cpp.o qvgmsplit.p/meson-generated_moc_options_dialog.cpp.o qvgmsplit.p/meson-generated_moc_render_dialog.cpp.o qvgmsplit.p/src_backend.cpp.o qvgmsplit.p/src_gui_app.cpp.o qvgmsplit.p/src_lib_format.cpp.o qvgmsplit.p/src_main.cpp.o qvgmsplit.p/src_mainwindow.cpp.o qvgmsplit.p/src_options_dialog.cpp.o qvgmsplit.p/src_render_dialog.cpp.o qvgmsplit.p/src_settings.cpp.o qvgmsplit.p/src_vgm.cpp.o qvgmsplit.p/src_wave_writer.cpp.o -Wl,--as-needed -Wl,--no-undefined -Wl,--start-group subprojects/libvgm/libvgm_audio.a subprojects/libvgm/libvgm_player.a subprojects/libvgm/libvgm_utils.a subprojects/STX/libstx.a -lQt5Widgets -lQt5Gui -lQt5Core -lpthread /usr/lib/libao.so /usr/lib/libasound.so /usr/lib/libc.so /usr/lib/libpulse.so /usr/lib/libz.so /usr/lib/libfmt.so -Wl,--end-group
/usr/bin/ld: qvgmsplit.p/src_backend.cpp.o: in function `Metadata::make(QByteArray, AppSettings const&)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../src/backend.cpp:160: undefined reference to `SndEmu_GetDevName'
/usr/bin/ld: qvgmsplit.p/src_vgm.cpp.o: in function `get_chip_metadata(PLR_DEV_INFO const&, bool)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../src/vgm.cpp:218: undefined reference to `SndEmu_GetDevName'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_droplayer.cpp.o): in function `DROPlayer::RefreshPanning(DROPlayer::DRO_CHIPDEV&, PLR_PAN_OPTS const&)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:462: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_droplayer.cpp.o): in function `DROPlayer::Start()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:686: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:693: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:714: undefined reference to `Resmpl_SetVals'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:720: undefined reference to `Resmpl_DevConnect'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:721: undefined reference to `Resmpl_Init'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_droplayer.cpp.o): in function `DROPlayer::Render(unsigned int, _waveform_32bit_stereo*)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/droplayer.cpp:901: undefined reference to `Resmpl_Execute'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_gymplayer.cpp.o): in function `GYMPlayer::DecompressZlibData()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:167: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_gymplayer.cpp.o): in function `GYMPlayer::RefreshPanning(GYMPlayer::GYM_CHIPDEV&, PLR_PAN_OPTS const&)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:407: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_gymplayer.cpp.o): in function `GYMPlayer::Start()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:621: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:628: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:646: undefined reference to `Resmpl_SetVals'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:647: undefined reference to `Resmpl_DevConnect'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:648: undefined reference to `Resmpl_Init'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_gymplayer.cpp.o): in function `GYMPlayer::Render(unsigned int, _waveform_32bit_stereo*)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:823: undefined reference to `Resmpl_Execute'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_gymplayer.cpp.o): in function `GYMPlayer::DoFileEnd()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/gymplayer.cpp:984: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::LoadFile(_data_loader*)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:257: undefined reference to `emu_logf'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:263: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::LoadTags()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:346: undefined reference to `emu_logf'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:347: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o):/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:357: more undefined references to `emu_logf' follow
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::RefreshPanning(S98Player::S98_CHIPDEV&, PLR_PAN_OPTS const&)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:655: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::GenerateDeviceConfig()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:836: undefined reference to `SndEmu_GetDevName'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::Start()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:957: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:964: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:989: undefined reference to `Resmpl_SetVals'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:996: undefined reference to `Resmpl_DevConnect'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:997: undefined reference to `Resmpl_Init'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::Reset()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:1063: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:1064: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::Render(unsigned int, _waveform_32bit_stereo*)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:1158: undefined reference to `Resmpl_Execute'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::HandleEOF()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:1196: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_s98player.cpp.o): in function `S98Player::DoCommand()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/s98player.cpp:1242: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o): in function `VGMPlayer::ParseHeader()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:267: undefined reference to `emu_logf'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:300: undefined reference to `emu_logf'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:323: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o):/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:329: more undefined references to `emu_logf' follow
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o): in function `VGMPlayer::RefreshPanning(VGMPlayer::CHIP_DEVICE&, PLR_PAN_OPTS const&)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:643: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o): in function `VGMPlayer::InitDevices()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1355: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1358: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1368: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1371: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1372: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1373: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1376: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1379: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1380: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1381: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1382: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1383: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1386: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1389: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1390: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1391: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1392: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1393: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1397: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1400: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1403: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1406: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1407: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1408: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1409: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1412: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1415: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1416: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1417: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1428: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1431: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1432: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1433: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1434: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1447: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1450: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1451: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1454: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1457: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1458: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1459: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1462: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1465: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1466: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1467: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1473: undefined reference to `SndEmu_Start'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1476: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1477: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1478: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1479: undefined reference to `SndEmu_GetDeviceFunc'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1490: undefined reference to `SndEmu_GetDevName'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1542: undefined reference to `Resmpl_SetVals'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1543: undefined reference to `Resmpl_DevConnect'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1544: undefined reference to `Resmpl_Init'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o): in function `VGMPlayer::SeekToFilePos(unsigned int)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1702: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o): in function `VGMPlayer::Render(unsigned int, _waveform_32bit_stereo*)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1742: undefined reference to `Resmpl_Execute'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer.cpp.o): in function `VGMPlayer::ParseFile(unsigned int)':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer.cpp:1799: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_helper.c.o): in function `SetupLinkedDevices':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/helper.c:39: undefined reference to `SndEmu_Start'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_helper.c.o): in function `FreeDeviceTree':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/helper.c:63: undefined reference to `Resmpl_Deinit'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/helper.c:64: undefined reference to `SndEmu_Stop'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/helper.c:66: undefined reference to `SndEmu_FreeDevLinkData'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_invalid()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:535: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_unknown()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:543: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_EndOfData()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:559: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DataBlock()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:728: undefined reference to `emu_logf'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:730: undefined reference to `emu_logf'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o):/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:732: more undefined references to `emu_logf' follow
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DACCtrl_Setup()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:872: undefined reference to `device_start_daccontrol'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:897: undefined reference to `daccontrol_setup_chip'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DACCtrl_SetData()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:914: undefined reference to `daccontrol_set_data'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DACCtrl_SetFrequency()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:926: undefined reference to `daccontrol_set_frequency'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DACCtrl_PlayData_Loc()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:941: undefined reference to `daccontrol_start'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DACCtrl_Stop()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:953: undefined reference to `daccontrol_stop'
/usr/bin/ld: /home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:964: undefined reference to `daccontrol_stop'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_DACCtrl_PlayData_Blk()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:988: undefined reference to `daccontrol_start'
/usr/bin/ld: subprojects/libvgm/libvgm_player.a(player_vgmplayer_cmdhandler.cpp.o): in function `VGMPlayer::Cmd_RF5C_Mem()':
/home/nyanpasu64/code/qvgmsplit-meson/builddir/../subprojects/libvgm/player/vgmplayer_cmdhandler.cpp:1151: undefined reference to `emu_logf'
collect2: error: ld returned 1 exit status
ninja: build stopped: subcommand failed.
```

- So many warnings. So many false positives. Help.
