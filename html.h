const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>SoyoSource Web-Limiter</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #263c5e; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .card1 { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .card-grid { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.0rem; }
    .headline { font-size: 2.0rem; font-weight: bold; }
    .card.info { color: #034078; }
    .card.info-dark { color: #151515; }
    .card-title { font-size: 1.2rem; font-weight: bold; color: #034078; }
    .card-title-dark { font-size: 1.2rem; font-weight: bold; color: #151515; }
    .btn:active { transform: scale(0.80); box-shadow: 3px 2px 22px 1px rgba(0, 0, 0, 0.24); }
    .btn {
            text-decoration: none;
            border: none;
            width: 90px;
            padding: 12px 20px;
            margin: 5px 5px 15px 5px;
            text-align: center;
            font-size: 16px;
            background-color:#034078;
            color: #fff;
            border-radius: 5px;
            box-shadow: 7px 6px 28px 1px rgba(0, 0, 0, 0.24);
            cursor: pointer;
            outline: none;
            transition: 0.2s all;
          }
  </style>
</head>
<body>

  <div class="topnav">
    <h3>SoyoSource Web-Limiter</h3>
  </div>

  <div class="content">
    <div class="card1">
      <div class="card info-dark">
        <p class="card-title-dark">ESP Infos</p>
        <div>
          <table style="width:50%">
            <tr>
              <td style="text-align: left">ClientID:</td>
              <td style="text-align: left"><span id="client_id">%CLIENTID%</span></td>
            </tr>
            <tr>
              <td style="text-align: left">Wifi RSSI:</td>
              <td style="text-align: left"><span><span id="wifi_rssi">%WIFIRSSI%</span> dB</span></td>
            </tr>
          </table>
        </div>
      </div>
    </div>
  </div>
  
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <p class="card-title"><span class="headline">AC Static: </span><span class="reading"><span id="static_power">%STATICPOWER%</span> W</span></p>
        <p class="card-title">Set Static Output Power</p>
          <table style="width:100%" class="center">
            <tr>
              <th><button type="button" onclick="minus1();" class="btn">- 1</button></th>
              <th></th>
              <th><button type="button" onclick="plus1();"  class="btn">+ 1</button></th>
            </tr>
            <tr>
              <th><button type="button" onclick="minus10();" class="btn">- 10</button></th>
              <th><button type="button" onclick="set_0();"  class="btn">0</button></th>
              <th><button type="button" onclick="plus10();"  class="btn">+ 10</button></th>
            </tr>
          </table>
      </div>
    </div>
  </div>
  
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <table style="width:100%">
          <tr>
            <th style="text-align: right"><a href="update"><button type="button"  class="btn" style="width: auto; margin: 5px 5px 5px 5px; background-color:#767676;"> FW Update</button></a></th>
          </tr>
        </table>
      </div>
    </div>
  </div>

<script>

if (!!window.EventSource) {
 var source = new EventSource('/events');

 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);

 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('static_Power', function(e) {
  console.log("staticpower", e.data);
  document.getElementById('static_power').innerHTML = e.data;
 }, false);

 source.addEventListener('wifi_RSSI', function(e) {
  console.log("wifirssi", e.data);
  document.getElementById('wifi_rssi').innerHTML = e.data;
 }, false);

 source.addEventListener('client_ID', function(e) {
  console.log("clientid", e.data);
  document.getElementById('client_id').innerHTML = e.data;
 }, false);

}

function set_0() {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/set_0", true);
  xhttp.send();
}

function minus1 () {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/minus1", true);
  xhttp.send();
}

function minus10 () {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/minus10", true);
  xhttp.send();
}

function plus1 () {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/plus1", true);
  xhttp.send();
}

function plus10 () {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/plus10", true);
  xhttp.send();
}

</script>
</body>
</html>)rawliteral";