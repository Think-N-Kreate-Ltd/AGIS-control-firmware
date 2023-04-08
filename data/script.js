// Using Websocket for communication between server and clients
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);

// Request for data every interal
setInterval(get_data, 1000);

function onLoad(event) {
    initWebSocket();
    initButton();
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onWsOpen;
    websocket.onclose = onWsClose;
    websocket.onmessage = onWsMessage; // <-- add this line
}

function onWsOpen(event) {
    console.log('Connection opened');
}

function onWsClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onWsMessage(event) {
    console.log(event.data);

    // Parse JSON data and update
    const dataObj = JSON.parse(event.data);
    var numDrops = dataObj["NUM_DROPS"];
    var totalTime = dataObj["TOTAL_TIME"];
    var dripRate = dataObj["DRIP_RATE"];

    document.getElementById("num_drops_value").innerHTML = numDrops;
    document.getElementById("total_time_value").innerHTML = totalTime;
    document.getElementById("drip_rate_value").innerHTML = dripRate;
}

function initButton() {
    document.getElementById('get_data_btn').addEventListener('click', get_data);
}

function get_data() {
    websocket.send('GET_DATA_WS');
    console.log("Requested data from website")
}

// function get_drip_rate() {
//     websocket.send('GET_DRIP_RATE_WS');
//     console.log("Requested Drip Rate from website")
// }


/* -----------End Websocket------------- */


var DR = 0;
var drop_rate = 0;

// setInterval(function () { refreshtime1(); refreshtime2(); refreshno_of_drop(); refreshtotal_time(); refreshdrop_rate(); }, 1000);

// function refreshtime1() {
//     var xhttp = new XMLHttpRequest();
//     xhttp.onreadystatechange = function () {
//         document.getElementById("sendtime1").innerHTML = this.responseText;
//     };
//     xhttp.open("GET", "sendtime1", true);
//     xhttp.send();
// }

// function refreshtime2() {
//     var xhttp = new XMLHttpRequest();
//     xhttp.onreadystatechange = function () {
//         document.getElementById("sendtime2").innerHTML = this.responseText;
//         if (((this.responseText) === "not started") || ((this.responseText) === "no drop appears currently")) {
//             drop_rate = 0;
//         }
//         else {
//             drop_rate = 60000 / parseInt(this.responseText);
//         }
//     };
//     xhttp.open("GET", "sendtime2", true);
//     xhttp.send();
// }

// function refreshno_of_drop() {
//     var xhttp = new XMLHttpRequest();
//     xhttp.onreadystatechange = function () {
//         document.getElementById("sendno_of_drop").innerHTML = this.responseText;
//     };
//     xhttp.open("GET", "sendno_of_drop", true);
//     xhttp.send();
// }

// function refreshtotal_time() {
//     var xhttp = new XMLHttpRequest();
//     xhttp.onreadystatechange = function () {
//         document.getElementById("sendtotal_time").innerHTML = (parseFloat(this.responseText) / 1000).toString();
//     };
//     xhttp.open("GET", "sendtotal_time", true);
//     xhttp.send();
// }

// function refreshdrop_rate() {
//     var xhttp = new XMLHttpRequest();
//     xhttp.onreadystatechange = function () {
//         document.getElementById("senddrop_rate").innerHTML = drop_rate;
//     };
//     xhttp.open("GET", "senddrop_rate", true);
//     xhttp.send();
// }

function getValue() {
    setTimeout(function () {
        document.location.reload(false);
    }, 10);
}

function sendInput(element) {
    var xhr = new XMLHttpRequest();
    if (element.checked) { xhr.open("GET", "/get?input1=" + element.id, true); }
    else { xhr.open("GET", "/get?input1=STOP", true); }
    xhr.send();
}

function getDR() {
    if (Number.isInteger(parseInt((document.getElementById("AGIS1").value)))) {
        DR = (parseInt(document.getElementById("AGIS1").value)).toString();
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "/get?auto1=" + DR, true);
        xhttp.send();
    }
    else {
        alert("Please enter an integer");
    }
}

// var chartT = new Highcharts.Chart({
//     chart: { renderTo: 'chart-reading', marginRight: 10 },
//     time: { useUTC: false },
//     title: { text: 'Sensor Reading' },
//     plotOptions: {
//         line: {
//             animation: false,
//             dataLabels: { enabled: false }
//         }
//     },
//     accessibility: {
//         announceNewData: {
//             enabled: true,
//             minAnnounceInterval: 15000,
//             announcementFormatter: function (allSeries, newSeries, newPoint) {
//                 if (newPoint) {
//                     return 'New point added. Value: ' + newPoint.y;
//                 }
//                 return false;
//             }
//         }
//     },
//     xAxis: {
//         type: 'datetime', tickPixelInterval: 150
//     },
//     yAxis: {
//         title: { text: 'Drop detected' },
//         plotLines: [{ value: 0, width: 1, color: '#808080' }]
//     },
//     credits: { enabled: false },
//     legend: { enabled: true },
//     exporting: { enabled: false },
//     series: [{
//         name: 'AGIS1',
//         data: (function () {
//             var data = [], time = (new Date()).getTime(), i;
//             for (i = -19; i <= 0; i += 1) {
//                 data.push({
//                     x: time + i * 1000,
//                     y: 0
//                 });
//             }
//             return data;
//         }())
//     }]
// });

// var series = chartT.series[0];
// setInterval(function () { plotGraph(); }, 500);
// function plotGraph() {
//     var xhttp = new XMLHttpRequest();
//     xhttp.onreadystatechange = function () {
//         var sensorreading = parseInt(this.responseText);
//         var x = (new Date()).getTime(),
//             y = sensorreading;
//         chartT.series[0].addPoint([x, y], true, true);
//         document.getElementById("sendsensor").innerHTML = sensorreading;
//     };
//     xhttp.open("GET", "sendsensor", true);
//     xhttp.send();
// }