from pathlib import Path
import json
import re
import sys

here=Path(__file__).resolve().parent
root=Path(sys.argv[1] if len(sys.argv)>1 else 'S:/Github/talisman').resolve()
deps=Path(sys.argv[2] if len(sys.argv)>2 else 'C:/Users/Allen/OneDrive/Documents/ChatGPT/BBS/src/talisman/build/windows/vcpkg_installed/x86-windows').resolve()
(here/'build-inputs.json').write_text(json.dumps({'source':str(root),'dependencies':str(deps)},indent=2)+'\n')
text=(root/'Talisman/CMakeLists.txt').read_text()
sources=re.search(r'add_executable\(talisman (.*?)\)',text,re.S)[1].split()
sources=[(root/'Talisman'/name).resolve().as_posix() for name in sources]
cmake='''cmake_minimum_required(VERSION 3.24)
project(preserved_talisman_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(/EHsc)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(talisman
'''+''.join('  "'+s+'"\n' for s in sources)+''')
target_include_directories(talisman PRIVATE "'''+deps.as_posix()+'''/include" "'''+root.as_posix()+'''/Common")
target_compile_definitions(talisman PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(talisman PRIVATE /utf-8 /W3)
target_link_libraries(talisman PRIVATE
'''+''.join(' "'+deps.as_posix()+'/lib/'+lib+'.lib"\n' for lib in ['libcrypto','libssl','lua','sqlite3','ssh'])+''' bcrypt ws2_32 shlwapi)
add_executable(servo "'''+root.as_posix()+'''/Servo/Servo.cpp" "'''+root.as_posix()+'''/Servo/IPBlockItem.cpp" "'''+root.as_posix()+'''/Servo/EventMgr.cpp")
target_compile_definitions(servo PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(servo PRIVATE ws2_32)
add_executable(door-probe door-probe.cpp)
target_link_libraries(door-probe PRIVATE ws2_32)
'''
(here/'CMakeLists.txt').write_text(cmake)
print('Generated build wrapper using unmodified preserved source and available x86 dependency libraries.')
