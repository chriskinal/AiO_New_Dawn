// TouchFriendlyStyles.h
// CSS styles optimized for touchscreen tablets

#ifndef TOUCH_FRIENDLY_STYLES_H
#define TOUCH_FRIENDLY_STYLES_H

const char TOUCH_FRIENDLY_CSS[] PROGMEM = R"rawliteral(
/* Reset and base styles */
* {
    box-sizing: border-box;
    -webkit-tap-highlight-color: transparent;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    font-size: 18px; /* Larger base font for readability */
    line-height: 1.6;
    margin: 0;
    padding: 0;
    background: #f5f5f5;
    color: #333;
    -webkit-text-size-adjust: 100%; /* Prevent font scaling in landscape */
}

.container {
    max-width: 1024px;
    margin: 0 auto;
    padding: 15px;
}

/* Typography */
h1 {
    font-size: 28px;
    margin: 15px 0 10px;
    color: #2c3e50;
}

h2 {
    font-size: 20px;
    margin: 15px 0 10px;
    color: #34495e;
    border-bottom: 2px solid #ecf0f1;
    padding-bottom: 8px;
}

/* Touch-friendly navigation */
.nav-menu {
    list-style: none;
    padding: 0;
    margin: 20px 0;
}

.nav-menu li {
    margin: 10px 0;
}

.nav-menu a,
.touch-button {
    display: block;
    padding: 20px 25px;
    background: #3498db;
    color: white;
    text-decoration: none;
    border-radius: 10px;
    font-size: 20px;
    text-align: center;
    transition: all 0.2s;
    border: none;
    cursor: pointer;
    min-height: 60px; /* Ensure minimum touch target */
    display: flex;
    align-items: center;
    justify-content: center;
}

.nav-menu a:active,
.touch-button:active {
    background: #2980b9;
    transform: scale(0.98);
}

/* Status indicators */
.status {
    padding: 15px;
    background: #ecf0f1;
    border-radius: 10px;
    margin: 15px 0;
    font-size: 18px;
    text-align: center;
}

.status.success {
    background: #2ecc71;
    color: white;
}

.status.error {
    background: #e74c3c;
    color: white;
}

/* Form elements */
.form-group {
    margin: 15px 0;
}

label {
    display: block;
    font-size: 18px;
    margin-bottom: 10px;
    font-weight: 500;
}

input[type="text"],
input[type="number"],
input[type="password"],
select {
    width: 100%;
    padding: 15px;
    font-size: 18px;
    border: 2px solid #bdc3c7;
    border-radius: 8px;
    background: white;
    -webkit-appearance: none; /* Remove iOS styling */
}

input[type="text"]:focus,
input[type="number"]:focus,
input[type="password"]:focus,
select:focus {
    outline: none;
    border-color: #3498db;
}

/* Touch-friendly toggle switches */
.toggle-switch {
    position: relative;
    display: inline-block;
    width: 80px;
    height: 44px;
}

.toggle-switch input {
    opacity: 0;
    width: 0;
    height: 0;
}

.toggle-slider {
    position: absolute;
    cursor: pointer;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: #ccc;
    border-radius: 34px;
    transition: .3s;
}

.toggle-slider:before {
    position: absolute;
    content: "";
    height: 36px;
    width: 36px;
    left: 4px;
    bottom: 4px;
    background-color: white;
    border-radius: 50%;
    transition: .3s;
}

input:checked + .toggle-slider {
    background-color: #2ecc71;
}

input:checked + .toggle-slider:before {
    transform: translateX(36px);
}

/* Touch-friendly sliders */
input[type="range"] {
    -webkit-appearance: none;
    width: 100%;
    height: 44px;
    background: transparent;
    margin: 10px 0;
}

input[type="range"]::-webkit-slider-track {
    width: 100%;
    height: 8px;
    background: #bdc3c7;
    border-radius: 4px;
}

input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 44px;
    height: 44px;
    background: #3498db;
    border-radius: 50%;
    cursor: pointer;
    margin-top: -18px;
}

/* Grid layout for tablets */
.grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 15px;
    margin: 15px 0;
}

.card {
    background: white;
    border-radius: 10px;
    padding: 15px;
    box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    margin-bottom: 15px;
}

/* Responsive tables */
.table-container {
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
}

table {
    width: 100%;
    border-collapse: collapse;
    font-size: 16px;
}

th, td {
    padding: 15px;
    text-align: left;
    border-bottom: 1px solid #ecf0f1;
}

/* Loading spinner */
.spinner {
    display: inline-block;
    width: 40px;
    height: 40px;
    border: 4px solid #f3f3f3;
    border-top: 4px solid #3498db;
    border-radius: 50%;
    animation: spin 1s linear infinite;
}

@keyframes spin {
    0% { transform: rotate(0deg); }
    100% { transform: rotate(360deg); }
}

/* Status items in cards */
.status-item {
    display: flex;
    justify-content: space-between;
    padding: 10px 0;
    border-bottom: 1px solid #ecf0f1;
    font-size: 18px;
}

.status-item:last-child {
    border-bottom: none;
}

.status-item span:last-child {
    font-weight: 600;
    color: #2c3e50;
}

/* Info section */
.info {
    margin-top: 40px;
    padding: 20px;
    background: #ecf0f1;
    border-radius: 10px;
    text-align: center;
    font-size: 16px;
}

.info p {
    margin: 5px 0;
}

/* Media queries for different tablet sizes */
@media (max-width: 768px) {
    body {
        font-size: 16px;
    }
    
    h1 {
        font-size: 28px;
    }
    
    .container {
        padding: 15px;
    }
    
    .nav-menu a,
    .touch-button {
        padding: 15px 20px;
        font-size: 18px;
    }
}

/* Disable hover effects on touch devices */
@media (hover: none) {
    .nav-menu a:hover,
    .touch-button:hover {
        background: #3498db;
    }
}
)rawliteral";

#endif // TOUCH_FRIENDLY_STYLES_H