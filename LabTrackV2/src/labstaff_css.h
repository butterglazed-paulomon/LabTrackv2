#pragma once

const char labstaff_css[] PROGMEM = R"rawliteral(
:root {
  --bg-dark: #1e1e1e;
  --panel-bg: #2d2d2d;
  --text-color: #e0e0e0;
  --border-color: #444;
}

body {
  font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
  background-color: var(--bg-dark);
  color: var(--text-color);
  margin: 0;
  padding: 20px;
}

.container {
  max-width: 800px;
  margin: 0 auto;
  background-color: var(--panel-bg);
  padding: 20px;
  border-radius: 8px;
  box-shadow: 0 4px 10px rgba(0,0,0,0.3);
}

h2, h3 {
  text-align: center;
  color: #fff;
}

/* Prompt Box */
.scan-prompt {
  text-align: center;
  padding: 60px 20px;
  border: 2px dashed var(--border-color);
  border-radius: 10px;
  background: rgba(255, 255, 255, 0.02);
  margin-bottom: 20px;
}

.scan-prompt h3 {
  margin-top: 0;
  color: #aaa;
}

/* Transaction Details */
.transaction-box {
  display: none;
  animation: fadeIn 0.3s ease-in;
}

@keyframes fadeIn {
  from { opacity: 0; transform: translateY(-10px); }
  to { opacity: 1; transform: translateY(0); }
}

.transaction-box p {
  font-size: 1.1em;
  background: #383838;
  padding: 10px;
  border-radius: 4px;
  margin: 5px 0;
}

.transaction-box p strong {
  color: #bbb;
  display: inline-block;
  width: 120px;
}

/* Item List & Inspection Buttons */
.item-list {
  margin-top: 25px;
  border-top: 1px solid var(--border-color);
  padding-top: 15px;
}

.inspection-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background-color: #383838;
  padding: 12px 15px;
  margin-bottom: 10px;
  border-radius: 6px;
  border-left: 4px solid #555; /* Default gray border */
}

.inspection-row span {
  font-weight: bold;
  font-size: 1.1em;
}

.status-btns {
  display: flex;
  gap: 5px;
}

.status-btns button {
  padding: 8px 12px;
  border: none;
  border-radius: 4px;
  font-size: 0.9em;
  cursor: pointer;
  opacity: 0.4;
  transition: all 0.2s;
  font-weight: bold;
}

/* Button States */
.btn-good {
  background-color: #4CAF50;
  color: white;
}

.btn-bad {
  background-color: #f44336;
  color: white;
}

/* Active State (When Selected) */
.status-btns button.selected {
  opacity: 1;
  transform: scale(1.1);
  box-shadow: 0 0 8px rgba(0,0,0,0.5);
  border: 1px solid #fff;
}

/* Finalize Button */
.accept {
  width: 100%;
  margin-top: 30px;
  padding: 15px;
  font-size: 1.2em;
  font-weight: bold;
  background-color: #2196F3;
  color: white;
  border: none;
  border-radius: 5px;
  cursor: pointer;
  box-shadow: 0 4px 6px rgba(0,0,0,0.2);
}

.accept:hover {
  background-color: #1976D2;
}

.accept:disabled {
  background-color: #555;
  cursor: wait;
}

.footer {
  text-align: center;
  margin-top: 40px;
  border-top: 1px solid var(--border-color);
  padding-top: 15px;
  font-size: 0.9em;
}
)rawliteral";