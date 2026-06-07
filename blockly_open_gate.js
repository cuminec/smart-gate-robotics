// ============================================================
//  SECTION 1 — JSON Block Definition
// ============================================================

Blockly.defineBlocksWithJsonArray([
  {
    // ── Block identity ──────────────────────────────────────
    "type": "open_gate_with_speed",

    // ── Tooltip & help ──────────────────────────────────────
    "tooltip": "Opens the Smart Gate at a given speed. Speed ranges from 1 (slowest) to 10 (fastest).",
    "helpUrl": "",

    // ── Visual style ────────────────────────────────────────
    "colour": 160,          // Green hue — represents motion/gate action

    // ── Block label & inputs ─────────────────────────────────
    "message0": "Open Gate  speed %1",
    "args0": [
      {
        "type": "field_number",   // Numeric input field (inline, no separate block needed)
        "name": "SPEED",          // Internal key used by the generator below
        "value": 5,               // Default value shown in the block
        "min": 1,                 // Minimum allowed input
        "max": 10,                // Maximum allowed input
        "precision": 1            // Whole numbers only
      }
    ],

    // ── Connection shape ─────────────────────────────────────
    // previousStatement / nextStatement make this a "stack" block
    // (connects above and below like a standard command block)
    "previousStatement": null,
    "nextStatement": null
  }
]);


// ============================================================
//  SECTION 2 — JavaScript Code Generator
//  Maps the block's field values → actual servo code string.
//
//  Speed-to-angle mapping:
//    The servo.write() angle controls how fast the gate sweeps.
//    Speed 1  → small angle increment per tick (slow sweep)
//    Speed 10 → large angle increment (fast sweep)
//
//    Formula used:  servoAngle = speed * 9
//      Speed 1  → 9°   (gate barely opens)
//      Speed 5  → 45°  (half open)
//      Speed 10 → 90°  (fully open, maximum speed)
//
//  This maps cleanly onto the 0–90° range used in Part 1.
// ============================================================

javascript.javascriptGenerator.forBlock['open_gate_with_speed'] = function(block, generator) {

  // ── Read the speed value from the block field ────────────
  const speed = block.getFieldValue('SPEED');   // Returns a number 1–10

  // ── Clamp speed to valid range (defensive programming) ───
  const clampedSpeed = Math.min(10, Math.max(1, Number(speed)));

  // ── Convert speed to a servo angle ───────────────────────
  //    speed * 9 maps:  1→9°,  5→45°,  10→90°
  const servoAngle = clampedSpeed * 9;

  // ── Generate the output code string ──────────────────────
  //    This is the code that will be emitted into the final
  //    JavaScript / C++ program when the block is used.
  const code = [
    `// Open gate — speed level: ${clampedSpeed}/10`,
    `var speed = ${clampedSpeed};`,
    `var angle = speed * 9;  // Maps speed 1-10 → angle 9°-90°`,
    `servo.write(angle);     // servo.write(${servoAngle}) at speed ${clampedSpeed}`,
    `console.log("Gate opening at speed " + speed + " → servo angle: " + angle + "°");`
  ].join('\n');

  return code;
};


// ============================================================
//  SECTION 3 — Toolbox XML Snippet
// ============================================================

/*
<xml id="toolbox" style="display: none">
  <category name="Gate Control" colour="160">
    <block type="open_gate_with_speed">
      <field name="SPEED">5</field>
    </block>
  </category>
</xml>
*/


// ============================================================
//  SECTION 4 — Minimal Blockly Workspace Setup (HTML snippet)
//  Shows how to wire everything into a working Blockly page.
// ============================================================

/*
<!DOCTYPE html>
<html>
<head>
  <script src="https://unpkg.com/blockly/blockly_compressed.js"></script>
  <script src="https://unpkg.com/blockly/javascript_compressed.js"></script>
  <script src="https://unpkg.com/blockly/msg/en.js"></script>
  <script src="blockly_open_gate.js"></script>  <!-- This file -->
</head>
<body>
  <div id="blocklyDiv" style="height: 480px; width: 800px;"></div>

  <xml id="toolbox" style="display:none">
    <category name="Gate Control" colour="160">
      <block type="open_gate_with_speed"></block>
    </category>
  </xml>

  <script>
    const workspace = Blockly.inject('blocklyDiv', {
      toolbox: document.getElementById('toolbox')
    });

    // Generate and display code on every change
    workspace.addChangeListener(() => {
      const code = javascript.javascriptGenerator.workspaceToCode(workspace);
      console.log("Generated Code:\n", code);
    });
  </script>
</body>
</html>
*/
