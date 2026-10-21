"""Exercise the native preload fallback without saving project assets.

Run using UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
-EnablePlugins=PythonScriptPlugin -NullRHI -Unattended.
"""
import json
import tempfile
from pathlib import Path

import unreal

content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
state_class = unreal.load_class(None, '/LuaMachine/LuaTestState.LuaTestState_C')
assert state_class and world, 'A concrete LuaState class and editor world are required'
defaults = unreal.get_default_object(state_class)
properties = {
    'lua_filename': '',
    'lua_code_asset': None,
    'require_table': {'preload_probe': None},
    'add_project_content_dir_to_package_path': False,
    'append_project_content_dir_sub_dir': [],
}
original = {name: defaults.get_editor_property(name) for name in properties}
try:
    for name, value in properties.items():
        defaults.set_editor_property(name, value)
    state = unreal.LuaBlueprintFunctionLibrary.create_dynamic_lua_state(world, state_class)
finally:
    for name, value in original.items():
        defaults.set_editor_property(name, value)
assert state, 'Lua state initialization failed'
state.set_editor_property('require_table', {})
with tempfile.TemporaryDirectory(prefix='_preload_verification_', dir=content) as temporary:
    root = Path(temporary)
    first, second = root / 'first', root / 'second'
    first.mkdir()
    second.mkdir()
    (second / 'found.lua').write_text('return 73\n', encoding='utf-8')
    (first / 'broken.lua').write_text('error("expected_preload_error")\n', encoding='utf-8')
    (second / 'broken.lua').write_text('return 99\n', encoding='utf-8')
    (root / 'base.lua').write_text('return 41\n', encoding='utf-8')
    state.set_editor_property('append_project_content_dir_sub_dir', [str(first.relative_to(content)), str(second.relative_to(content))])

    def evaluate(expression):
        value = state.run_string(expression, 'package preload verification')
        assert value.get_editor_property('type') == unreal.LuaValueType.BOOL, 'Probe did not return a Lua boolean'
        assert value.get_editor_property('bool'), expression

    evaluate("return package.preload.preload_probe('found') == 73")
    evaluate("local ok, err = pcall(package.preload.preload_probe, 'missing'); return not ok and err:find('unable to find package missing', 1, true) ~= nil")
    evaluate("local ok, err = pcall(package.preload.preload_probe, 'broken'); return not ok and err:find('expected_preload_error', 1, true) ~= nil")
    state.set_editor_property('add_project_content_dir_to_package_path', True)
    key = str((root / 'base').relative_to(content))
    evaluate('return package.preload.preload_probe(' + json.dumps(key) + ') == 41')
unreal.log('LUAMACHINE_PRELOAD_VERIFICATION_PASSED: later search path, missing file, execution error, content root')
