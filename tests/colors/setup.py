from pathlib import Path
import re
import json
import sys
base=Path(__file__).resolve().parent
base.mkdir(parents=True,exist_ok=True)
if len(sys.argv)<2: raise SystemExit('Usage: python setup.py DEPENDENCY_ROOT [SOURCE_ROOT]')
deps=Path(sys.argv[1]).resolve()
root=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else base.parent.parent
text=(root/'Talisman/CMakeLists.txt').read_text()
sources=re.search(r'add_executable\(talisman (.*?)\)',text,re.S)[1].split()
sources=[(root/'Talisman'/x).as_posix() for x in sources if x!='main.cpp']
assert not any(x.endswith('/main.cpp') for x in sources)
cmake='''cmake_minimum_required(VERSION 3.24)
project(talisman_colors LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(/EHsc /utf-8 /W3)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
add_library(core STATIC
'''+''.join('"'+x+'"\n' for x in sources)+''')
target_include_directories(core PUBLIC "'''+deps.as_posix()+'''/include" "'''+root.as_posix()+'''/Common" "'''+root.as_posix()+'''/Talisman")
target_link_libraries(core PUBLIC
'''+''.join('"'+deps.as_posix()+'/lib/'+x+'.lib"\n' for x in ['libcrypto','libssl','lua','sqlite3','ssh'])+''' bcrypt ws2_32 shlwapi)
add_executable(color-probe color-probe.cpp)
target_link_libraries(color-probe PRIVATE core)
add_executable(talisman "'''+root.as_posix()+'''/Talisman/main.cpp")
target_link_libraries(talisman PRIVATE core)
'''
(base/'CMakeLists.txt').write_text(cmake)
(base/'build-inputs.json').write_text(json.dumps({'source':str(root),'dependencies':str(deps)},indent=2))
