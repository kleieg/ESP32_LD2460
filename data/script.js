var gateway = `ws://${window.location.host}/ws`;
var websocket;
window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    var message = "{\"card\":0,\"value\":0}";
    console.log(message);
    websocket.send(message);
}
  
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
} 

function onMessage(event) {
    var myObj = JSON.parse(event.data);
            console.log(myObj);
            for (i in myObj.cards){
                var c_text = myObj.cards[i].c_text;
                console.log(c_text);
                document.getElementById(i).innerHTML = c_text;
            }
    console.log(event.data);
}

function myReboot(id) {
    console.log("Reboot");
    var message = "{\"card\":" + id + ",\"value\":0}";
    websocket.send(message);
} 