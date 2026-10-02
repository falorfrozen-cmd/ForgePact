"""`droptrace` reads; it never decides whether a drop routine runs.

Live 1 rerun (issue #44, 2026-10-02): with `droptrace 20` armed, two boss
deaths (the rank-1 and the ancient Karp King) printed no `droptrace:` line,
no gold line and no item line, while a traced rare Karp King and a traced
slider-raised Skeleton_Mage_Fire_obj printed and dropped. These tests pin the
part of that question our code can answer: every DropManager hook enters the
probe scope first and then calls the original with its own arguments on
every path, so an armed trace cannot skip a drop or change one, and the
trace's own name read never hands the runner an unconvertible kind. What the
game decided for those two deaths is in `docs/boss-rarity-research.md`
("The plugin's runner error and the traced kills").
"""
from pathlib import Path
import re
import unittest

from test_map_reveal_behavior import implementation

ROOT = Path(__file__).resolve().parents[1]
DROP_MANAGER = ROOT / 'plugin/include/ForgePact/DropManager.hpp'
MODULE_MAIN = ROOT / 'plugin/ModuleMain.cpp'
KIND_CHECK = 'static bool IsNumericInstanceRead('


def drop_hook_macro():
    text = DROP_MANAGER.read_text(encoding='utf-8-sig')
    start = text.index('#define FP_DROP_HOOK(NAME)')
    end = text.index('#undef FP_DROP_HOOK', start)
    macro = text[start:end]
    end_of_define = macro.index('\n    FP_DROP_HOOK(')
    return macro[:end_of_define].replace('\\\n', '\n')


def hook_body(macro):
    head = 'static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {'
    start = macro.index(head) + len(head)
    return macro[start:macro.rindex('}')]


def statements(body):
    return [s.strip() for s in body.split(';') if s.strip()]


class DropTraceContractTests(unittest.TestCase):
    def test_drop_hook_calls_the_original_whatever_the_trace(self):
        body = hook_body(drop_hook_macro())
        steps = statements(body)
        # The probe scope (which carries `droptrace`) is entered first ...
        self.assertEqual(steps[0], 'BP_ANGELIC_PROBE_SCOPE(#NAME, S, argc, A)', steps)
        # ... and the original runs, unconditionally (outside every block), with the
        # hook's own arguments; the only return is its result.
        call = 'RValue& _res = mgr.m_Orig_##NAME ? mgr.m_Orig_##NAME(S, O, R, argc, A) : R;'
        self.assertEqual(body.count(call), 1, body)
        at = body.index(call)
        self.assertEqual(body[:at].count('{'), body[:at].count('}'), 'the original call sits inside a block')
        self.assertEqual(body.count('return'), 1, body)
        self.assertEqual(steps[-1], 'return _res', steps)
        # The only branch is the multiplier loop's null check on the original itself:
        # nothing branches on the scope, the trace or any other result.
        self.assertEqual(re.findall(r'\bif\s*\(([^)]*)\)', body), ['mgr.m_Orig_##NAME'], body)
        self.assertNotIn('_apRollScope', body)
        self.assertNotIn('DropTrace', body)
        for arg in ('S', 'O', 'R', 'argc', 'A'):
            self.assertNotRegex(body, r'(?<![\w.>])' + arg + r'\s*=(?!=)', 'the hook reassigns ' + arg)

        source = MODULE_MAIN.read_text(encoding='utf-8-sig')
        # The scope is a declaration in the research build and nothing in the player build:
        # neither can return from the hook body.
        for define in ('#define BP_ANGELIC_PROBE_SCOPE(name, s, argc, a) ((void)0)',
                       '#define BP_ANGELIC_PROBE_SCOPE(name, s, argc, a) ApRollDropScope _apRollScope(name, s, argc, a)'):
            self.assertTrue(define in source, define)
        scope = implementation(source, 'struct ApRollDropScope {')
        self.assertIn(': raised(ApRollDropScopeEnter(hookName, S, argc, A)) {}', scope)
        self.assertIn('~ApRollDropScope() { if (raised) --g_ApRollDropItemDepth; }', scope)
        # The trace and the scope's entry read the arguments and return nothing the hook uses.
        self.assertTrue('static void DropTraceNote(const char* hookName, CInstance* S, int argc, RValue** A)' in source)
        for signature in ('static void DropTraceNote(', 'static bool ApRollDropScopeEnter(const char* hookName, CInstance* S, int argc, RValue** A)\n{',
                          'static std::string TyArgs('):
            fn = re.sub(r'"(?:\\.|[^"\\])*"', '""', implementation(source, signature))   # string literals out
            self.assertNotRegex(fn, r'\*\s*A\s*\[[^\]]*\]\s*=(?!=)', signature)
            self.assertNotRegex(fn, r'(?<![\w.>])(argc|A|S)\s*=(?!=)', signature)

    def test_trace_name_read_checks_the_kind(self):
        source = MODULE_MAIN.read_text(encoding='utf-8-sig')
        self.assertTrue(KIND_CHECK in source, 'no named kind check for the instance reads')
        check = implementation(source, KIND_CHECK)
        kinds = set(re.findall(r'VALUE_[A-Z0-9]+', check))
        self.assertEqual(kinds, {'VALUE_REAL', 'VALUE_INT32', 'VALUE_INT64', 'VALUE_REF'}, check)
        # Defined before its first user, the enemy-born guard's caller read (whose
        # definition is the last occurrence; a forward declaration sits above it).
        self.assertLess(source.index(KIND_CHECK), source.rindex('static int CallerObjectIndex('))

        name = implementation(source, 'static std::string TyInstName(')
        self.assertIn('IsNumericInstanceRead(oi)', name)
        self.assertLess(name.index('IsNumericInstanceRead(oi)'), name.index('object_get_name'))
        self.assertIn('IsNumericInstanceRead(id)', name)
        self.assertLess(name.index('IsNumericInstanceRead(id)'), name.index('id.ToDouble()'))
        for signature, value in (('static int CallerObjectIndex(', 'oi'), ('static double InstanceIdOf(', 'v')):
            fn = implementation(source, signature)
            self.assertLess(fn.index('IsNumericInstanceRead(' + value + ')'), fn.index(value + '.ToDouble()'), signature)


if __name__ == '__main__':
    unittest.main()
