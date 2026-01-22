#pragma once

const char borrowform_css[] PROGMEM = R"rawliteral(
:root {
  --bg-dark: #222;
  --panel-bg: #333;
  --text-color: #fff;
  --accent-blue: #2196F3;
  --accent-green: #4CAF50;
  --accent-red: #f44336;
  --accent-orange: #FF9800;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background-color: var(--bg-dark);
  color: var(--text-color);
  margin: 0;
  padding: 20px;
}

.container {
  max-width: 600px;
  margin: 0 auto;
  background-color: var(--panel-bg);
  padding: 25px;
  border-radius: 10px;
  box-shadow: 0 4px 15px rgba(0,0,0,0.5);
}

h2 {
  text-align: center;
  margin-top: 0;
  border-bottom: 1px solid #555;
  padding-bottom: 15px;
}

label {
  display: block;
  margin-top: 15px;
  margin-bottom: 5px;
  font-weight: bold;
  font-size: 0.9em;
  color: #ccc;
}

input, textarea {
  width: 100%;
  padding: 12px;
  border-radius: 5px;
  border: 1px solid #555;
  background-color: #444;
  color: #fff;
  box-sizing: border-box; /* Fix padding issues */
  font-size: 1em;
}

input:focus, textarea:focus {
  outline: none;
  border-color: var(--accent-blue);
  background-color: #555;
}

/* Shopping Cart Styles */
.item-row {
  display: flex;
  gap: 10px;
}

.item-row button {
  width: auto;
  padding: 0 20px;
  background-color: var(--accent-blue);
  color: white;
  border: none;
  border-radius: 5px;
  cursor: pointer;
  font-weight: bold;
}

.cart-container {
  margin-top: 15px;
  padding: 10px;
  background: rgba(0, 0, 0, 0.2);
  border: 1px dashed #555;
  border-radius: 5px;
  min-height: 60px;
  display: flex;
  flex-wrap: wrap;
  align-content: flex-start;
}

.cart-item {
  background-color: var(--accent-blue);
  color: white;
  padding: 6px 12px;
  margin: 4px;
  border-radius: 20px;
  font-size: 0.9em;
  cursor: pointer;
  transition: background 0.2s, transform 0.1s;
  user-select: none;
}

.cart-item:hover {
  background-color: var(--accent-red);
  transform: scale(1.05);
}

.cart-item::after {
  content: " ×";
  font-weight: bold;
  margin-left: 5px;
}

/* Main Action Button */
.flat-button {
  display: block;
  width: 100%;
  margin-top: 25px;
  padding: 15px;
  font-size: 1.1em;
  font-weight: bold;
  background-color: var(--accent-green);
  color: white;
  border: none;
  border-radius: 5px;
  cursor: pointer;
  transition: filter 0.2s;
}

.flat-button:hover {
  filter: brightness(1.1);
}

.flat-button:disabled {
  background-color: #555;
  cursor: not-allowed;
  color: #888;
}

/* Status Messages */
#status-msg {
  margin-top: 20px;
  padding: 15px;
  border-radius: 5px;
  text-align: center;
  font-weight: bold;
  display: none;
}

.status-wait {
  background-color: var(--accent-orange);
  color: white;
  animation: pulse 1.5s infinite;
}

.status-error {
  background-color: var(--accent-red);
  color: white;
}

.status-success {
  background-color: var(--accent-green);
  color: white;
}

@keyframes pulse {
  0% { opacity: 1; }
  50% { opacity: 0.7; }
  100% { opacity: 1; }
}

.footer {
  margin-top: 30px;
  text-align: center;
  font-size: 0.85em;
  border-top: 1px solid #444;
  padding-top: 15px;
}
)rawliteral";