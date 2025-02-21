const express = require('express');
const { createClient } = require('@supabase/supabase-js');
const bodyParser = require('body-parser');
require('dotenv').config();

const app = express();
app.use(bodyParser.json());
app.use(express.static('public'));

// Supabase client initialization
const supabaseUrl = 'https://yaumextpxkxbbrdczypq.supabase.co';
const supabaseKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlhdW1leHRweGt4YmJyZGN6eXBxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Mzk3MDg5ODgsImV4cCI6MjA1NTI4NDk4OH0.khiXPj6b6hG3i5zoqod9cPJKQ5bFj2OK7c7i5iMYW2I';
const supabase = createClient(supabaseUrl, supabaseKey);

// Store system state
let systemState = {
  pumpActive: false,
  lastIrrigation: null,
  autoMode: true,
  moistureThreshold: 300,
  lastUpdate: new Date(),
  error: null
};

// Middleware to log all requests
app.use((req, res, next) => {
  console.log(`${new Date().toISOString()} - ${req.method} ${req.url}`);
  next();
});

// Endpoint to receive soil data from ESP32
app.post('/api/soil_data', async (req, res) => {
  try {
    const { soil_moisture, temperature, humidity, rainfall } = req.body;

    if (!soil_moisture || !temperature || !humidity) {
      throw new Error('Missing required sensor data');
    }

    // Store data in Supabase
    const { data, error } = await supabase
      .from('sensor_readings')
      .insert([{
        soil_moisture,
        temperature,
        humidity,
        rainfall: rainfall || 0,
        timestamp: new Date()
      }]);

    if (error) throw error;

    // Update system state
    systemState.lastUpdate = new Date();
    systemState.error = null;

    // Automatic irrigation control based on moisture
    if (systemState.autoMode && soil_moisture < systemState.moistureThreshold) {
      systemState.pumpActive = true;
      systemState.lastIrrigation = new Date();
    }

    res.status(200).json({
      message: 'Data received successfully',
      systemState
    });
  } catch (error) {
    console.error('Error processing sensor data:', error);
    systemState.error = error.message;
    res.status(500).json({ error: error.message });
  }
});

// Get system state and latest readings
app.get('/api/status', async (req, res) => {
  try {
    const { data: latestReading, error } = await supabase
      .from('sensor_readings')
      .select('*')
      .order('timestamp', { ascending: false })
      .limit(1)
      .single();

    if (error) throw error;

    res.json({
      systemState,
      latestReading
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Endpoint for ESP32 to fetch irrigation commands
app.get('/api/irrigation_control', (req, res) => {
  res.json({
    activate_pump: systemState.pumpActive,
    timestamp: new Date()
  });
});

// Update system settings
app.post('/api/settings', (req, res) => {
  try {
    const { autoMode, moistureThreshold } = req.body;

    if (typeof autoMode === 'boolean') {
      systemState.autoMode = autoMode;
    }

    if (typeof moistureThreshold === 'number') {
      systemState.moistureThreshold = moistureThreshold;
    }

    res.json({ message: 'Settings updated', systemState });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Manual pump control
app.post('/api/pump', (req, res) => {
  try {
    const { activate } = req.body;

    if (typeof activate !== 'boolean') {
      throw new Error('Invalid pump control command');
    }

    systemState.pumpActive = activate;
    systemState.lastIrrigation = activate ? new Date() : systemState.lastIrrigation;
    systemState.autoMode = false; // Disable auto mode when manual control is used

    res.json({ message: 'Pump control updated', systemState });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Serve the main dashboard
app.get('/', (req, res) => {
  res.send(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Smart Irrigation Dashboard</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
      <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
      <style>
        .card { margin-bottom: 20px; }
        .status-indicator {
          width: 15px;
          height: 15px;
          border-radius: 50%;
          display: inline-block;
          margin-right: 5px;
        }
        .status-active { background-color: #28a745; }
        .status-inactive { background-color: #dc3545; }
      </style>
    </head>
    <body>
      <div class="container mt-4">
        <h1>Smart Irrigation Dashboard</h1>
        <!-- System Status and Controls -->
        <div class="row mt-4">
          <div class="col-md-6">
            <div class="card">
              <div class="card-header">System Status</div>
              <div class="card-body">
                <div class="d-flex justify-content-between align-items-center mb-2">
                  <span>Pump Status:</span>
                  <div>
                    <span id="pumpIndicator" class="status-indicator status-inactive"></span>
                    <span id="pumpStatus">Inactive</span>
                  </div>
                </div>
                <div class="d-flex justify-content-between align-items-center mb-2">
                  <span>Mode:</span>
                  <span id="systemMode">Automatic</span>
                </div>
                <div class="d-flex justify-content-between align-items-center">
                  <span>Last Updated:</span>
                  <span id="lastUpdate">-</span>
                </div>
              </div>
            </div>
          </div>
          <div class="col-md-6">
            <div class="card">
              <div class="card-header">Latest Readings</div>
              <div class="card-body">
                <div id="latestReadings">Loading...</div>
              </div>
            </div>
          </div>
        </div>
        <!-- Controls and Settings -->
        <div class="row mt-4">
          <div class="col-md-6">
            <div class="card">
              <div class="card-header">Manual Controls</div>
              <div class="card-body">
                <button id="activatePump" class="btn btn-success me-2">Activate Pump</button>
                <button id="deactivatePump" class="btn btn-danger">Deactivate Pump</button>
              </div>
            </div>
          </div>
          <div class="col-md-6">
            <div class="card">
              <div class="card-header">System Settings</div>
              <div class="card-body">
                <div class="form-check mb-3">
                  <input type="checkbox" class="form-check-input" id="autoMode">
                  <label class="form-check-label" for="autoMode">Automatic Mode</label>
                </div>
                <div class="mb-3">
                  <label for="moistureThreshold" class="form-label">Moisture Threshold</label>
                  <input type="range" class="form-range" id="moistureThreshold" min="0" max="1000">
                  <span id="thresholdValue">300</span>
                </div>
                <button id="saveSettings" class="btn btn-primary">Save Settings</button>
              </div>
            </div>
          </div>
        </div>
        <!-- Charts -->
        <div class="row mt-4">
          <div class="col-12">
            <div class="card">
              <div class="card-header">Sensor History</div>
              <div class="card-body">
                <canvas id="sensorChart"></canvas>
              </div>
            </div>
          </div>
        </div>
      </div>
      <script>
        // JavaScript for the dashboard
        // (Include your existing JavaScript code here)
      </script>
    </body>
    </html>
  `);
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
  console.log(`Dashboard available at http://localhost:${PORT}`);
});