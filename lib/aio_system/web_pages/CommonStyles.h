// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CommonStyles.h
// Common CSS styles for all web pages

#ifndef COMMON_STYLES_H
#define COMMON_STYLES_H

#include <Arduino.h>

const char COMMON_CSS[] PROGMEM = R"rawliteral(
body {
    font-family: Arial, sans-serif;
    margin: 20px;
    background-color: #f0f0f0;
}
.container {
    max-width: 800px;
    margin: 0 auto;
    background-color: white;
    padding: 20px;
    border-radius: 10px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}
h1 {
    color: #333;
}
h2 {
    color: #555;
}
.status {
    margin: 10px 0;
    padding: 10px;
    background-color: #e8f5e9;
    border-radius: 5px;
}
.form-group {
    margin: 15px 0;
}
label {
    display: inline-block;
    width: 150px;
    font-weight: bold;
}
select, input[type="number"], input[type="text"] {
    padding: 5px;
    width: 200px;
}
button {
    background-color: #4CAF50;
    color: white;
    padding: 10px 20px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
    margin: 5px;
}
button:hover {
    background-color: #45a049;
}
.btn {
    padding: 10px 20px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
    margin: 5px;
    color: white;
    text-decoration: none;
    display: inline-block;
}
.btn-home {
    background-color: #2196F3;
}
.btn-home:hover {
    background-color: #1976D2;
}
.btn-primary {
    background-color: #4CAF50;
}
.btn-primary:hover {
    background-color: #45a049;
}
.btn-warning {
    background-color: #ff9800;
}
.btn-warning:hover {
    background-color: #e68900;
}
.btn-danger {
    background-color: #f44336;
}
.btn-danger:hover {
    background-color: #da190b;
}
.btn-secondary {
    background-color: #757575;
}
.btn-secondary:hover {
    background-color: #616161;
}
.nav-buttons {
    margin-top: 20px;
    display: flex;
    flex-wrap: wrap;
    gap: 5px;
}
.info {
    background-color: #e8f5e9;
    padding: 10px;
    border-radius: 5px;
    margin: 10px 0;
}
.help-text {
    color: #f44336;
    font-size: 0.9em;
    margin-left: 10px;
}
a {
    color: #2196F3;
    text-decoration: none;
}
a:hover {
    text-decoration: underline;
}
)rawliteral";

#endif // COMMON_STYLES_H