# Model tests

The profile format is plain C++17 and can be tested without a TAB5:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Imain main/remote_model.cpp tests/remote_model_test.cpp -o remote_model_test
./remote_model_test
c++ -std=c++17 -Wall -Wextra -Werror -Imain main/remote_model.cpp main/ir_decoder.cpp main/keyboard_keys.cpp tests/input_decode_test.cpp -o input_decode_test
./input_decode_test
c++ -std=c++17 -Wall -Wextra -Werror -Imain main/keyboard_keys.cpp main/usb_keyboard_report.cpp tests/usb_keyboard_report_test.cpp -o usb_keyboard_report_test
./usb_keyboard_report_test
```

Alternatively compile with Emscripten and run the resulting JavaScript with Node:

```sh
em++ -std=c++17 -Wall -Wextra -Werror -Imain main/remote_model.cpp tests/remote_model_test.cpp -o build-tab5/remote_model_test.js
node build-tab5/remote_model_test.js
em++ -std=c++17 -Wall -Wextra -Werror -Imain main/remote_model.cpp main/ir_decoder.cpp main/keyboard_keys.cpp tests/input_decode_test.cpp -o build-tab5/input_decode_test.js
node build-tab5/input_decode_test.js
em++ -std=c++17 -Wall -Wextra -Werror -Imain main/keyboard_keys.cpp main/usb_keyboard_report.cpp tests/usb_keyboard_report_test.cpp -o build-tab5/usb_keyboard_report_test.js
node build-tab5/usb_keyboard_report_test.js
```

Hardware acceptance checks:

1. Connect Unit IR to Grove. Create two differently named profiles.
2. Learn Power in the first profile, test, then save it. Learn a different Power message in the second profile.
3. Switch profiles and select Power. Send should use each profile's own message.
4. Reboot and check both names and messages survive.
5. Learn a replacement, then discard. The previous saved message must still send.
6. Cancel learning and allow a separate learning attempt to time out. Both should leave stored codes intact.
7. Clear one button and delete one profile using their confirmation screens. Reboot and verify.
8. Try a held remote button, optical noise and an oversized frame; malformed captures should not be saved automatically.
9. Attach the TAB5 Keyboard to Ext.Port1. Type a mixed-case profile name, edit with Backspace, save with Enter, and cancel another name with Esc.
10. Use arrows/Tab to select buttons, Ctrl+Left/Right to select remotes, Ctrl+L to learn, T to test, Enter to save, and Enter again to send. Esc cancels a pending capture without saving it.
11. Confirm Delete/Ctrl+D still require Enter confirmation and Esc abandons deletion. Disconnect the keyboard: touch must remain usable. Reattach and wait up to two seconds for detection.
12. Learn a known NEC remote frame and compare displayed address/command with the remote's documented code. Reboot and select that saved assignment: decoded values must remain the same. Confirm an unknown-protocol capture is labelled as raw.
13. Check the display is rotated 180 degrees from the previous firmware, then touch each corner and several buttons to confirm rotated touch alignment.
14. Plug a standard USB boot keyboard into USB-A, both before boot and while running. Check the USB header changes to connected, type and edit a name, then exercise the same shortcuts as the TAB5 attachment.
15. Hold Enter on a saved message: it should send once per physical press. Check Shift, Caps Lock and Ctrl shortcuts. Unplug while holding a key, reconnect and verify the first press is recognized normally.
16. Use USB and Ext.Port1 keyboards together; disconnect each independently and confirm the other and touch input still work. Try a composite keyboard/mouse: only its keyboard interface should affect the app.
