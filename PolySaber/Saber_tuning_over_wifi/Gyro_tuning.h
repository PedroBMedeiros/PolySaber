// HTML to render the plot

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Saber Tuner</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background: #1a1a1a; color: #eee; margin: 0; padding: 10px; }
    canvas { background: #000; border: 2px solid #444; width: 100%; max-width: 800px; height: auto; cursor: crosshair; }
    .legend { display: flex; justify-content: center; gap: 20px; margin-top: 10px; font-size: 0.9em; }
    .label { display: flex; align-items: center; gap: 5px; }
    .box { width: 12px; height: 12px; border-radius: 2px; }
  </style>
</head><body>
  <h3>Saber Tuning Dashboard</h3>
  <canvas id="cvs" width="800" height="400"></canvas>
  
  <div class="legend">
    <div class="label"><div class="box" style="background:red"></div> Accel (m/s²)</div>
    <div class="label"><div class="box" style="border: 1px dashed red"></div> Strike Limit</div>
    <div class="label"><div class="box" style="background:cyan"></div> Gyro (rad/s)</div>
    <div class="label"><div class="box" style="border: 1px dashed cyan"></div> Swing Limit</div>
  </div>

  <script>
    const canvas = document.getElementById('cvs');
    const ctx = canvas.getContext('2d');
    const STRIKE_VAL = 19.0;
    const SWING_VAL = 1.5;
    let dataLog = [];

    // Scaling Factors to fit 400px height
    const ACCEL_SCALE = 15; // 20m/s2 * 15 = 300px
    const GYRO_SCALE = 150; // 2rad/s * 150 = 300px

    const gateway = `ws://${window.location.hostname}/ws`;
    let websocket = new WebSocket(gateway);

    websocket.onmessage = (event) => {
      dataLog.push(JSON.parse(event.data));
      if(dataLog.length > 130) dataLog.shift();
      draw();
    };

    function draw() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      // 1. Draw Grid / Thresholds
      ctx.lineWidth = 1;
      
      // Strike Threshold (Red Dashed)
      ctx.strokeStyle = "rgba(255, 0, 0, 0.5)";
      ctx.setLineDash([5, 5]);
      let strikeY = canvas.height - (STRIKE_VAL * ACCEL_SCALE);
      ctx.beginPath(); ctx.moveTo(0, strikeY); ctx.lineTo(canvas.width, strikeY); ctx.stroke();
      
      // Swing Threshold (Cyan Dashed)
      ctx.strokeStyle = "rgba(0, 255, 255, 0.5)";
      let swingY = canvas.height - (SWING_VAL * GYRO_SCALE);
      ctx.beginPath(); ctx.moveTo(0, swingY); ctx.lineTo(canvas.width, swingY); ctx.stroke();
      
      ctx.setLineDash([]); // Reset dash for data lines

      // 2. Draw Accel Data (Red)
      ctx.strokeStyle = "red"; ctx.lineWidth = 2;
      ctx.beginPath();
      dataLog.forEach((d, i) => {
        let x = i * (canvas.width / 130);
        let y = canvas.height - (d.a * ACCEL_SCALE);
        i == 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
      });
      ctx.stroke();

      // 3. Draw Gyro Data (Cyan)
      ctx.strokeStyle = "cyan";
      ctx.beginPath();
      dataLog.forEach((d, i) => {
        let x = i * (canvas.width / 130);
        let y = canvas.height - (d.g * GYRO_SCALE);
        i == 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
      });
      ctx.stroke();
    }
  </script>
</body></html>)rawliteral";


