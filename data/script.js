// Some define that can be quicky changed by developer
const GET_INFUSION_MONITORING_DATA_INTERVAL = 1000; // in milliseconds, avoid setting this number too small
const DRIP_RATE_NOT_SET = -1;

// Using Websocket for communication between server and clients
var gateway = `ws://${location.host}/ws`;
var websocket;
window.addEventListener('load', onLoad);

// for logging
var logRoute = "log";

// Variables for infusion
var vtbi;
var total_time_hours;
var total_time_minutes;
var drop_factor_obj;
var target_drip_rate = DRIP_RATE_NOT_SET;


// NOTE:(1) make sure infusionState_t type is the same as in main.cpp
// otherwise, it won't work
// NOTE:(2) Javascript enum declaration is not the same as in C :)
const infusionState_t = Object.freeze({
    NOT_STARTED: "NOT_STARTED",
    STARTED: "STARTED",
    IN_PROGRESS: "IN_PROGRESS",
    ALARM_COMPLETED: "ALARM_COMPLETED",
    ALARM_STOPPED: "ALARM_STOPPED",
    ALARM_VOLUME_EXCEEDED: "ALARM_VOLUME_EXCEEDED"
});

var infusionState;

function onLoad(event) {
    initWebSocket();
    // initButton();
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

    // Request for data every interal
    // NOTE: only request for data after the WebSocket is opened
    // Why doing this? We can actually use smallest possible interval and get
    // the new data asap. However, it is not necessary to do so, and to avoid
    // taking more resources from both the clients (browsers) and server (ESP32)
    setInterval(getInfusionMonitoringData, GET_INFUSION_MONITORING_DATA_INTERVAL);
}

function onWsClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onWsMessage(event) {
    // console.log("Received data:")
    // console.log(event.data);

    // Parse JSON data
    const dataObj = JSON.parse(event.data);
    infusionState = dataObj["INFUSION_STATE"];

    if (infusionState === infusionState_t.NOT_STARTED) {
        let text = "Not started";
        document.getElementById("time_1_drop_value").innerHTML = text;
        document.getElementById("time_btw_2_drops_value").innerHTML = text;
        document.getElementById("num_drops_value").innerHTML = text;
        document.getElementById("total_time_value").innerHTML = text;
        document.getElementById("drip_rate_value").innerHTML = text;
        document.getElementById("infused_volume_value").innerHTML = text;
        document.getElementById("infused_time_value").innerHTML = text;
        document.getElementById("infusion_state_value").style.color = "black"
        document.getElementById("infusion_state_value").innerHTML = text;
    }
    //  only update when there are drops
    else if ((infusionState === infusionState_t.STARTED) ||
        (infusionState === infusionState_t.IN_PROGRESS) ||
        (infusionState === infusionState_t.ALARM_COMPLETED)) {
        var time1Drop = dataObj["TIME_1_DROP"];
        var time_btw_2_drops = dataObj["TIME_BTW_2_DROPS"];
        var numDrops = dataObj["NUM_DROPS"];
        var totalTime = dataObj["TOTAL_TIME"];
        var dripRate = dataObj["DRIP_RATE"];
        var infusedVolume = dataObj["INFUSED_VOLUME"];
        var infusedVolumeRounded = parseFloat(infusedVolume).toFixed(2);
        var infusedTime = convertSecondsToHHMMSS(dataObj["INFUSED_TIME"]);

        document.getElementById("time_1_drop_value").innerHTML = time1Drop;
        document.getElementById("time_btw_2_drops_value").innerHTML = time_btw_2_drops;
        document.getElementById("num_drops_value").innerHTML = numDrops;
        document.getElementById("total_time_value").innerHTML = totalTime;
        document.getElementById("drip_rate_value").innerHTML = dripRate;
        document.getElementById("infused_volume_value").innerHTML = infusedVolumeRounded;
        document.getElementById("infused_time_value").innerHTML = infusedTime;

        if (infusionState === infusionState_t.STARTED) {
            document.getElementById("infusion_state_value").style.color = "black"
            document.getElementById("infusion_state_value").innerHTML = "Drops are detected";
        }

        else if (infusionState === infusionState_t.IN_PROGRESS) {
            document.getElementById("infusion_state_value").style.color = "blue"
            document.getElementById("infusion_state_value").innerHTML = "In progress";

            document.getElementById("download_infusion_data_btn").disabled = true;
        }

        else if (infusionState === infusionState_t.ALARM_COMPLETED) {
            document.getElementById("infusion_state_value").style.color = "green"
            document.getElementById("infusion_state_value").innerHTML = "ALARM: infusion has completed";

            // allow download infusion data only if it has completed
            document.getElementById("download_infusion_data_btn").disabled = false;

        }
    }
    else if (infusionState === infusionState_t.ALARM_STOPPED) {
        let text = "No recent drop";
        document.getElementById("time_1_drop_value").innerHTML = text;
        document.getElementById("time_btw_2_drops_value").innerHTML = text;
        document.getElementById("total_time_value").innerHTML = text;
        document.getElementById("drip_rate_value").innerHTML = text;
        document.getElementById("infusion_state_value").style.color = "red"
        document.getElementById("infusion_state_value").innerHTML = "ALARM: no recent drop";
    }
    else if (infusionState === infusionState_t.ALARM_VOLUME_EXCEEDED) {
        document.getElementById("infusion_state_value").style.color = "red"
        document.getElementById("infusion_state_value").innerHTML = "ALARM: volume exceeded";
    }
    else {
        console.log("infusionState value not recognized");
    }
}

// function initButton() {
//     document.getElementById('get_data_btn').addEventListener('click', getInfusionMonitoringData);
// }

function getInfusionMonitoringData() {
    const msg = {
        COMMAND: "GET_INFUSION_MONITORING_DATA_WS",
    };
    websocket.send(JSON.stringify(msg));
}


/* -----------End Websocket------------- */

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

function setTargetDripRate() {
    if (target_drip_rate != DRIP_RATE_NOT_SET) {
        const msg = {
            SET_VTBI_WS: vtbi,
            SET_TOTAL_TIME_HOURS_WS: total_time_hours,
            SET_TOTAL_TIME_MINUTES_WS: total_time_minutes,
            SET_DROP_FACTOR_WS: drop_factor_obj.value,
            SET_TARGET_DRIP_RATE_WS: target_drip_rate
            // date: Date.now(),  // may need later
        };
        websocket.send(JSON.stringify(msg));
        console.log(JSON.stringify(msg));

        return true;
    }
    else {
        alert("Please fill in all inputs");
        return false;
    }
}

function setTargetDripRateAndRun() {
    if (setTargetDripRate()) {
        // send a WebSocket message to enable autoControl()
        const msg = {
            COMMAND: "ENABLE_AUTOCONTROL_WS",
        };
        websocket.send(JSON.stringify(msg));
        console.log(JSON.stringify(msg));
    }
}

// Call this function when receiving user inputs
function calculateTargetDripRate() {
    vtbi = parseInt(document.getElementById("VTBI").value);
    total_time_hours = parseInt(document.getElementById("total_time_hours").value);
    total_time_minutes = parseInt(document.getElementById("total_time_minutes").value);
    drop_factor_obj = document.querySelector('input[name="drop_factor"]:checked');

    // check if all inputs satisfy
    if (Number.isInteger(vtbi) && Number.isInteger(total_time_hours)
        && Number.isInteger(total_time_minutes) && (drop_factor_obj != null)) {

        // Calculate drip rate based on formular
        target_drip_rate = vtbi / (total_time_hours * 60 + total_time_minutes) * drop_factor_obj.value;
        // TODO: handle boundary cases
        document.getElementById("drip_rate").style.color = "green"
        document.getElementById("drip_rate").innerHTML = target_drip_rate.toString();
    }
    // TODO: handle unsatisfy inputs
}

function convertSecondsToHHMMSS(seconds) {
    var hhmmss;
    if (seconds < 3600) {
        // if seconds are less than 1 hour and you only need mm:ss
        hhmmss = new Date(seconds * 1000).toISOString().slice(14, 19);
    }
    else {
        // get hh:mm:ss string
        hhmmss = new Date(seconds * 1000).toISOString().slice(11, 19);
    }
    return hhmmss;
}

function downloadInfusionData() {
    window.open(logRoute);
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