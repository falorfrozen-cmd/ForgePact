"""Contract tests for `menuprobe`, the character-select research instrument.

Whether anything inside this process can drive the game's own main menu as
far as a loaded character is unmeasured (docs/character-select-research.md,
the live session pending). This stage ships nothing a player can reach: a
research-build verb, and the document the session fills in.

These tests pin the three properties that would otherwise rot quietly.

1. **The verb never reaches a player build.** `menuprobe` performs game
   events and calls game scripts with a hand-picked instance as self. That is
   a research surface, and the mechanical proof it stays one is that the
   literal disappears when the research blocks are stripped.
2. **One call per command, behind a literal word, with state printed either
   side of it.** A loop around `event_perform` or a second call shape is how
   a probe stops being a measurement, and a call that prints only
   success/failure flattens "refused", "ran and did nothing" and "faulted"
   into one outcome - `AGENTS.md`, "Prove the Instrument Before Trusting a
   Negative Result".
3. **Names, never addresses.** Every instance is reached through
   `asset_get_index`, `instance_find` and `HhResolveInstance`; nothing in the
   new code may hook anything or compute a target.

Modelled on test_prospect_window_contract.py, which pins the same three
properties for `prospectprobe` - the precedent this verb follows down to the
dispatch line, because RunCommand's else-if chain is at MSVC's nesting limit.
"""
import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
DOC = ROOT / "docs" / "character-select-research.md"
PANEL = ROOT / "src" / "forgepact.py"

# One definition of "what the player build compiles", shared with the release
# contract rather than copied, so the two can never disagree about it.
_spec = importlib.util.spec_from_file_location(
    "_release_hook_contract", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
function_body = _release.function_body
strip_comments = _release.strip_comments

#: The three steps by which a name becomes an instance a call can take as
#: self. Asserted as a set over the instrument's helpers rather than per
#: function, because which helper does which step is an implementation
#: detail and "one of them resolves an address instead" is not.
RESOLUTION_STEPS = ("asset_get_index", "instance_find", "HhResolveInstance")

#: Ways of reaching game code that this instrument must not contain. `Rva`
#: and `GetModuleHandle` are the hand-resolved-address shape `AGENTS.md`
#: forbids outright; the two hook installers are how a probe turns into a
#: mod without anyone deciding to.
FORBIDDEN = ("Rva", "GetModuleHandle", "MmCreateHook", "HookOneScript")

#: Every heading the research document must carry, in the order a reader
#: needs them: what was searched, what is being compared, what does the
#: measuring, how, what came back, what was already known to be negative,
#: and the two lines the shipping work reads.
DOC_HEADINGS = (
    "## Static search",
    "## Candidates and controls",
    "## Instrument",
    "## Live procedure",
    "## Results",
    "## Negative results, sourced",
    "## Decision",
)

#: Every name `hs-game-sdk` already has for this path. The document's static
#: search has to name all of them, because the expensive mistake here is a
#: later session re-deriving a list that was already written down.
SDK_NAMES = (
    "Main_Menu_rm", "Char_Select_rm", "Login_rm", "Game_Start_rm",
    "Town_01_rm",
    "UiAMainMenuLocal", "UiAMainMenuOnline", "UiAMainMenuOptions",
    "UiAMainMenuExit", "UiAChooseSaveSlot", "UiAChooseSaveSlotPage",
    "UiACharacterPlay", "UiACreateCharacter", "UiACharacterDelete",
    "UiACharacterDeleteConfirm", "LoadSlot", "GameStart",
    "Menu_Controller_obj", "Profile_Manager_obj", "Select_Parent_obj",
    "Select_Random_obj", "Load_Inventory_Char_Select_obj",
    "Save_Character_obj", "Save_Slot_Shop_obj", "UI_Button_obj",
)

#: The five existing verbs that read as though they might already do this.
#: Each is something else, and the document says which - so the next reader
#: does not spend a session finding that out again.
REJECTED_VERBS = ("forceslot", "forcelogin", "puppetinput", "roomprobe",
                  "coopstart")

#: The candidate ids the research document measures, and the only tokens the
#: shipping workorder will accept in its `finding:` line. `bc` is deliberately
#: absent: the event and script routes are measured separately because their
#: controls are separate, and one can pass while the other fails - which is
#: exactly what happened on 2026-09-21.
CANDIDATES = ("a-sendinput", "a-postmessage", "d", "bc-event", "bc-script")

#: How a candidate's `## Decision` line may open. `works` is a result,
#: `not observed` is a result scoped to what was tried, and `unmeasured`
#: means the route's own control failed and nothing it reported counts.
VERDICTS = ("works", "not observed", "unmeasured")


def section(doc, heading):
    """From a line that is exactly `heading` to the next `## ` heading."""
    doc = doc.replace("\r\n", "\n")
    start = doc.index("\n" + heading + "\n") + 1
    following = doc.find("\n## ", start + len(heading))
    return doc[start:] if following < 0 else doc[start:following]


def flowed(text):
    """Markdown prose with its line wrapping taken out.

    A sentence this test pins can sit across a line break, and re-wrapping a
    paragraph is not a change in what it says - so the assertions read the
    text the way a reader does, not the way the file stores it.
    """
    return re.sub(r"\s+", " ", text.replace("\r\n", "\n").replace("*", ""))


def live_step(doc, number):
    """One numbered step of `## Live procedure`, up to the next number."""
    procedure = section(doc, "## Live procedure")
    start = re.search(r"(?m)^%d\. " % number, procedure)
    assert start, "the live procedure has no step %d" % number
    following = re.search(r"(?m)^%d\. " % (number + 1), procedure)
    return procedure[start.start():following.start() if following else len(procedure)]


class MenuProbeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8")
        cls.doc = DOC.read_text(encoding="utf-8")
        cls.handler = function_body(cls.plugin, "static bool HandleMenuProbeCommand(")
        cls.command = function_body(cls.plugin, "static void MpCommand(")
        cls.event = function_body(cls.plugin, "static void MpEvent(")
        cls.script = function_body(cls.plugin, "static void MpScript(")
        cls.listing = function_body(cls.plugin, "static void MpList(")
        cls.resolve = function_body(cls.plugin, "static bool MpResolve(")
        cls.where = function_body(cls.plugin, "static std::string MpWhere(")
        # Everything the research block contains, header comment included -
        # the comment is as much part of the instrument as the code, because
        # it is what the next reader believes about where this runs.
        cls.block = cls.plugin[
            cls.plugin.index("// Research instrument for docs/character-select-research.md"):
            cls.plugin.index("#endif // FORGEPACT_RELEASE (menuprobe)")]

    def helpers(self):
        """Every function the instrument is made of, as one blob."""
        return "\n".join([self.handler, self.command, self.event, self.script,
                          self.listing, self.resolve,
                          function_body(self.plugin, "static std::string MpWhere("),
                          function_body(self.plugin, "static std::string MpVar("),
                          function_body(self.plugin, "static std::string MpRoom("),
                          function_body(self.plugin, "static RValue MpArg("),
                          function_body(self.plugin, "static void MpUsage(")])

    # ---- the instrument never reaches a player ------------------------------

    def test_menuprobe_is_research_build_only(self):
        shipped = strip_research_blocks(self.plugin)
        self.assertIn("menuprobe", self.plugin)
        self.assertNotIn("menuprobe", shipped)
        self.assertNotIn("MpCommand(", shipped)
        self.assertNotIn("MpEvent(", shipped)
        self.assertNotIn("MpScript(", shipped)

    def test_menuprobe_absent_from_player_commands(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertNotIn("menuprobe", block)
        # PR #60 was folded into PR #61 (owner decision, 2026-09-21): the one
        # menu verb a player build answers is #61's read-only `menulayout`.
        # Any other menu-named entry is still a research verb leaking out.
        menu_verbs = sorted(set(re.findall(r'"(\w*menu\w*)"', block)))
        self.assertEqual(menu_verbs, ["menulayout"])

    def test_nothing_player_visible_exists_yet(self):
        # Research stage: no panel row, no toggle, no shipped command that
        # selects a character. That belongs to the follow-on workorder (the
        # hub's hs_select_character); `menulayout`, folded in beside this
        # verb, only reports where the buttons are.
        panel = PANEL.read_text(encoding="utf-8")
        self.assertNotIn("menuprobe", panel)
        self.assertNotIn("charselect", panel)
        self.assertNotIn("charselect", self.plugin)

    def test_menuprobe_dispatched_from_handle_menu_probe_command(self):
        # RunCommand's else-if chain is at MSVC's nesting limit (C1061), so
        # the verb lives in its own handler called straight after the other
        # two, and the literal appears exactly once in the whole file.
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn(
            "if (HandleHeadhunterCommand(lc, rest)) return;\n"
            "    if (HandleProspectCommand(lc, rest)) return;\n"
            "    if (HandleMenuProbeCommand(lc, rest)) return;",
            run.replace("\r\n", "\n"))
        self.assertIn('lc == "menuprobe"', self.handler)
        self.assertEqual(self.plugin.count('"menuprobe"'), 1)
        self.assertNotIn('"menuprobe"', run)

    def test_no_new_top_level_else_if_in_run_command(self):
        """The chain's length is the thing C1061 is about.

        Counted against the tree as it was before this change (`f5a3515`),
        so adding a verb the wrong way fails here rather than at the
        compiler, which reports the limit hundreds of lines from the cause.
        """
        run = strip_comments(function_body(
            self.plugin, "static void RunCommand(const std::string& line)"))
        self.assertEqual(len(re.findall(r"(?m)^\s{4}\}\s*else if \(", run)), 121)

    def test_nothing_reaches_the_frame_callback(self):
        frame = function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        self.assertNotIn("menuprobe", frame)
        self.assertEqual(re.findall(r"\bMp[A-Z]\w*", frame), [],
                         "a probe that runs every frame is a mod, not a probe")

    def test_the_comments_say_where_a_command_actually_runs(self):
        """The safety argument is that this *does* run on the frame thread.

        Both the header comment and the research document used to say
        `menuprobe` does not run from `FrameCallback`. Every ForgePact command
        runs inside `PollCommands()`, which `FrameCallback` calls every 30
        frames - which is exactly what makes `CallBuiltinEx` and
        `CallGameScriptEx` safe here. A reader who believes the old sentence
        would look for a safety argument that does not exist, and could
        conclude from the stall watchdog's comment that calling into the
        runtime from a command is the same risk. The narrower true claim -
        nothing is installed on the per-frame path - is the test above.
        """
        # Scoped to the comment block and the doc's own Instrument section,
        # not the whole file/doc: the retracted claim lived there, and a
        # file-wide ban would forbid the phrase anywhere else in either text
        # for reasons that have nothing to do with this claim.
        self.assertNotIn("nothing runs from the frame callback", self.block,
                         "the header comment claims the opposite of the truth")
        self.assertIn("PollCommands", self.block,
                      "the header comment has to name where its command runs")
        instrument = section(self.doc, "## Instrument")
        self.assertNotIn("nothing runs from the frame callback", instrument)
        self.assertIn("PollCommands", instrument,
                      "the document's Instrument section names it too")

    # ---- one call per command, behind the literal word ----------------------

    def test_event_and_script_check_confirm_before_any_call(self):
        for name, body, call in (("MpEvent", self.event, "CallBuiltinEx("),
                                 ("MpScript", self.script, "CallGameScriptEx(")):
            with self.subTest(function=name):
                stripped = strip_comments(body)
                gate = stripped.index('Lower(token) != "confirm"')
                self.assertLess(gate, stripped.index(call),
                                f"{name} reaches the game before it checks confirm")
                # The refusal has to be a usage line, not a bare return: a
                # command that fires nothing and says nothing reads exactly
                # like a command that fired and did nothing.
                refusal = stripped[gate:stripped.index("}", gate)]
                self.assertIn("Usage ->", refusal)
                self.assertIn("return;", refusal)

    def test_list_requires_no_token(self):
        self.assertNotIn("confirm", strip_comments(self.listing))
        self.assertIn('sub == "list"', self.command)
        # ...and the read-only subcommand is dispatched without one.
        branch = self.command[self.command.index('sub == "list"'):]
        self.assertNotIn("confirm", branch[:branch.index("\n")])

    def test_each_calling_function_makes_exactly_one_call_and_never_loops(self):
        for name, body, call in (("MpEvent", self.event, "CallBuiltinEx("),
                                 ("MpScript", self.script, "CallGameScriptEx(")):
            with self.subTest(function=name):
                stripped = strip_comments(body)
                self.assertEqual(stripped.count(call), 1,
                                 f"{name} must reach the game exactly once")
                self.assertEqual(stripped.count("CallGameScript("), 0)
                for keyword in ("for (", "while ("):
                    self.assertNotIn(keyword, stripped,
                                     f"{name} encloses its one call in a loop")

    def test_the_before_and_after_lines_name_what_moved(self):
        for name, body in (("MpEvent", self.event), ("MpScript", self.script)):
            with self.subTest(function=name):
                self.assertIn("before:", body)
                self.assertIn("after:", body)
                self.assertEqual(body.count("MpRoom()"), 2,
                                 "the room index is printed before and after")
                self.assertEqual(body.count("MpWhere("), 2)
                self.assertIn("EXCEPTION", body,
                              "a fault must print as a fault, not as silence")
        where = self.where
        for field in ("nth=", '"id"', '"x"', '"y"'):
            self.assertIn(field, where)
        self.assertIn("objName", where, "the object is named on the line")
        self.assertIn("Describe(res)", self.event)
        self.assertIn("Describe(res)", self.script)

    def test_a_destroyed_instance_does_not_print_like_a_missing_variable(self):
        """`MpVar` answers "" for both, so the after line has to ask first.

        `variable_instance_exists` is false whether the instance never
        carried that variable or the call just destroyed it, so an after line
        built only out of `MpVar` prints the same empty `id= x= y=` for both -
        collapsing the one distinction the before/after split exists to make.
        `MpList` already marks a handle it cannot resolve; this is the
        equivalent for the handle a call may have killed.
        """
        self.assertIn("instance_exists", self.where,
                      "the after line has to ask whether the instance is "
                      "still there before it reads variables off it")
        self.assertIn("<destroyed>", self.where,
                      "one marker, not three empty fields")

    # ---- names, never addresses ---------------------------------------------

    def test_instances_are_resolved_by_name_in_three_steps(self):
        helpers = self.helpers()
        for step in RESOLUTION_STEPS:
            self.assertIn(step, helpers, f"{step} is how a name becomes an instance")
        self.assertIn("HhResolveInstance", self.resolve,
                      "the handle must be proven live before it is used as self")

    def test_the_instrument_hooks_nothing_and_resolves_no_address(self):
        helpers = self.helpers()
        for token in FORBIDDEN:
            self.assertNotIn(token, helpers,
                             f"{token} has no business in a read-and-one-call probe")

    # ---- the research document ----------------------------------------------

    def test_the_document_carries_every_heading(self):
        text = self.doc.replace("\r\n", "\n")
        found = [heading for heading in DOC_HEADINGS
                 if "\n" + heading + "\n" in text]
        self.assertEqual(found, list(DOC_HEADINGS),
                         "a missing or renamed heading; the shipping "
                         "workorder reads this document by section")

    def test_the_document_is_crlf_like_every_other_forgepact_doc(self):
        raw = DOC.read_bytes()
        self.assertGreater(raw.count(b"\r\n"), 0)
        self.assertEqual(raw.count(b"\n") - raw.count(b"\r\n"), 0)

    def test_the_static_search_names_every_sdk_symbol(self):
        search = section(self.doc, "## Static search")
        missing = [name for name in SDK_NAMES if name not in search]
        self.assertEqual(missing, [],
                         "the static search must name what the SDK already "
                         "has, or the next session re-derives it")

    def test_the_static_search_says_what_the_five_near_misses_actually_are(self):
        search = section(self.doc, "## Static search")
        for verb in REJECTED_VERBS:
            self.assertIn(verb, search,
                          f"{verb} reads as though it might already do this; "
                          "the document has to say what it really is")

    def test_the_decision_lines_are_present_and_answered(self):
        """Both lines are present, answered, and answered from measurement.

        These assertions replaced a pair that required the literal `pending`,
        deliberately, when the live session ran on 2026-09-21 - that pair
        existed so an invented finding could not be mistaken for a measured
        one before there was anything to measure. The protection has to
        survive the change rather than be dropped with it, so what is checked
        now is the rule itself: a candidate may be named in `finding:` only if
        this document also records that it *worked*. A candidate whose own
        control failed is `unmeasured`, and `unmeasured` is not a result.
        """
        text = self.doc.replace("\r\n", "\n")
        self.assertEqual(len(re.findall(r"(?m)^finding: ", text)), 1)
        self.assertEqual(len(re.findall(r"(?m)^shipRoute: ", text)), 1)
        finding = re.search(r"(?m)^finding: (.+)$", text).group(1).strip()
        ship = re.search(r"(?m)^shipRoute: (.+)$", text).group(1).strip()
        self.assertNotEqual(finding, "pending",
                            "the session has run; `pending` is no longer an "
                            "answer this document may give")
        self.assertNotEqual(ship, "pending")
        self.assertIn(ship, ("mcp-only", "forgepact-player", "research-dll",
                             "none"),
                      "the shipping workorder selects its branch from this "
                      "token and stops on anything it does not know")

        named = [] if finding == "none" else [t.strip()
                                              for t in finding.split(",")]
        self.assertTrue(named or finding == "none")
        for token in named:
            self.assertIn(token, CANDIDATES,
                          f"{token!r} is not one of the candidates this "
                          "document measured")
            label = self._candidate_label(token)
            self.assertTrue(label.startswith("works"),
                            f"{token} is named in `finding:` but its own "
                            f"line is labelled {label!r}; a candidate whose "
                            "control did not pass has measured nothing "
                            "(AGENTS.md, 'Prove the Instrument')")

    def test_every_candidate_carries_exactly_one_labelled_line(self):
        decision = section(self.doc, "## Decision")
        bullets = [line for line in decision.split("\n")
                   if line.startswith("* `")]
        self.assertEqual(len(bullets), len(CANDIDATES),
                         "one line per candidate, no more and no fewer")
        for candidate in CANDIDATES:
            label = self._candidate_label(candidate)
            self.assertTrue(
                any(label.startswith(v) for v in VERDICTS),
                f"{candidate}'s line is labelled {label!r}; the label has to "
                f"open with one of {VERDICTS}, so a reader and this test see "
                "the same verdict")
        for overclaim in ("does not happen", "never fires", "impossible",
                          "does not work", "cannot work"):
            self.assertNotIn(overclaim, self.doc,
                             f"'{overclaim}' is a claim measurement does not "
                             "support; negatives here are 'not observed' and "
                             "are scoped to what was actually tried")

    def _candidate_label(self, candidate):
        """The bolded verdict opening `candidate`'s `## Decision` line.

        The verdict is a *position* - the `**...**` immediately after the
        dash - not any phrase appearing somewhere in the bullet. A bullet
        legitimately cites other steps' findings in its prose (`d`'s line
        explains that keyboard navigation is not observed, which is C-1.11's
        result and not `d`'s verdict), so matching a substring anywhere in
        the paragraph reads a citation as a verdict.
        """
        decision = section(self.doc, "## Decision").replace("\r\n", "\n")
        # DOTALL: a verdict that names what it is scoped to can be long
        # enough to wrap, and a wrapped label is still a label.
        match = re.search(r"(?ms)^\* `%s` - \*\*(.+?)\*\*"
                          % re.escape(candidate), decision)
        self.assertIsNotNone(
            match,
            f"{candidate} needs a `## Decision` line of the form "
            "``* `<id>` - **<verdict>.**``")
        return " ".join(match.group(1).split()).rstrip(".")

    def test_the_results_table_has_a_row_per_live_step(self):
        results = section(self.doc, "## Results")
        for step in range(1, 23):
            self.assertIn("| C-1.%d |" % step, results,
                          "every step of the live procedure needs somewhere "
                          "to put its reply, written before the session")

    def test_the_document_states_the_enumeration_control(self):
        """(b) and (c) can only be measured if the menu's instances enumerate.

        Recording that as a control, in advance, is what stops an empty
        `menuprobe list` being written up later as "the events do nothing".
        """
        instrument = section(self.doc, "## Instrument")
        self.assertIn("enumeration control", instrument)
        candidates = section(self.doc, "## Candidates and controls")
        self.assertIn("positive control", candidates)
        for candidate in ("a-sendinput", "a-postmessage", "bc-event",
                          "bc-script"):
            self.assertIn(candidate, candidates)

    def test_the_enumeration_control_is_the_same_instrument(self):
        """`menuprobe list UI_Button_obj` cannot be its own control.

        An empty listing is equally consistent with "the buttons are a
        different object" and with "nothing in a menu room survives the read
        path `list` uses", which has never been shown either way on this
        runner. `citrace dumpobj` does not settle it: it resolves an instance
        identically to `menuprobe list` (both run `asset_get_index` ->
        `instance_number` -> `instance_find` -> `HhResolveInstance`) but reads
        it differently once resolved - `dumpobj` walks the raw `CInstance*`;
        `list` goes through `variable_instance_exists`/`variable_instance_get`
        instead - and a negative is only worth anything against the
        instrument that produced it. So the control is a
        `menuprobe list` of an object the same step's `dumpobj` shows live,
        read as a pair, and the document has to say which outcome pair is
        which - `AGENTS.md`, "Prove the Instrument Before Trusting a Negative
        Result".
        """
        doc = self.doc.replace("\r\n", "\n")
        control = "menuprobe list Menu_Controller_obj"
        self.assertGreaterEqual(
            doc.count(control), 2,
            "the control is read once with the other menu reads and again "
            "beside the measurement it qualifies")
        for number in (6, 13):
            step = live_step(self.doc, number)
            self.assertIn(control, step,
                          "step %d reads the control" % number)
        step13 = live_step(self.doc, 13)
        self.assertIn("UI_Button_obj", step13)
        self.assertIn("Profile_Manager_obj", step13,
                      "the fallback control object, for a menu where the "
                      "first one has no instances")
        self.assertIn("both empty measures the instrument", flowed(step13),
                      "the outcome pair that means (b) and (c) were never "
                      "measured has to be written down before the session, "
                      "not decided afterwards")
        self.assertIn("control: fail", step13)

        results = section(self.doc, "## Results")
        row = [line for line in results.split("\n") if line.startswith("| C-1.13 |")]
        self.assertEqual(len(row), 1)
        self.assertIn("UI_Button_obj", row[0])
        self.assertIn("Menu_Controller_obj", row[0],
                      "the Results row records the control beside the "
                      "reading it qualifies, or the pair is lost")

        instrument = section(self.doc, "## Instrument")
        self.assertIn("both empty measures the instrument", flowed(instrument))
        self.assertIn(control, instrument)

        # D25/F3: the control's justification changed - `dumpobj` and `list`
        # resolve an instance identically, so the claim they take "two
        # different paths to an instance" is false and must not reappear,
        # in either text, scoped to where the claim actually lives (never
        # over the whole plugin file or the whole document).
        self.assertNotIn("different paths to an instance", self.block)
        self.assertNotIn("different paths to an instance", instrument)
        self.assertIn("variable_instance_get", instrument,
                      "the corrected reason names the read path that "
                      "actually differs")

    def test_step_13s_middle_branch_names_a_next_action(self):
        """F5: the middle branch used to leave the session to improvise.

        Control non-empty, `UI_Button_obj` empty was recorded as
        `control: pass` with no next action stated - and calling (c)
        "measured" in that branch was itself an over-read, because step 14's
        own positive control hardcoded the same object, so no script call
        ran there either. The repair names a fall-to object (step 6 already
        found `Select_Parent_obj` live) and makes step 14's control follow
        whichever object step 13 actually used.
        """
        step13 = live_step(self.doc, 13)
        step14 = live_step(self.doc, 14)
        self.assertIn("Select_Parent_obj", step13,
                      "step 13's middle branch has to name a fall-to object")
        self.assertIn("re-run this step and step 14", flowed(step13))
        self.assertNotIn("not-this-object", step13,
                         "the over-read the middle branch is not allowed "
                         "to make again")
        self.assertIn("the object that listed instances in step 13",
                      flowed(step14),
                      "step 14's control must not hardcode the same object "
                      "step 13's middle branch found empty")
        self.assertIn("C-1.14", step14,
                      "the doc records that the hardcoded control already "
                      "raised, so a reader knows why a re-run needs a "
                      "different control script")
        self.assertIn("control script already proven to return a known "
                      "value", flowed(step14))

    def test_step_16_arms_orbpickup_and_reads_the_negative_control(self):
        """F8: `orbpickup stat` only answers while `orbpickup` is armed.

        The session that produced C-1.16 never sent `orbpickup 1`, and
        `g_OrbPlayerHow` is written only inside `FrameCallback`'s
        `orbpickup`-on branch - so that row measured the off state, not a
        general failure of the field. A re-run has to arm the mod, read
        `none` at the menu as the negative control, then read again in town.
        """
        step16 = live_step(self.doc, 16)
        self.assertIn("orbpickup 1", step16)
        self.assertIn("none", step16)
        results = section(self.doc, "## Results")
        row = [line for line in results.split("\n") if line.startswith("| C-1.16 |")]
        self.assertEqual(len(row), 1)
        self.assertNotIn("never attempted", row[0],
                         "the row must not claim the field never answers, "
                         "only that this session never armed it")

    def test_the_document_sources_each_negative_it_relies_on(self):
        negatives = section(self.doc, "## Negative results, sourced")
        for source in ("pet-quest-collector-c-research.md",
                       "pet-quest-collector-b4-research.md",
                       "pet-quest-collector-plan.md"):
            self.assertIn(source, negatives,
                          "a negative without its source is read later as "
                          "settled fact by whoever finds it")
        self.assertIn("not observed", negatives)


if __name__ == "__main__":
    unittest.main()
