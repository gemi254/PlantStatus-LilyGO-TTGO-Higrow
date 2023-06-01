PROGMEM const char HTML_CHARTS_CSS[] = R"~(
<style>
body { margin: 2px; padding:2px; } 
html, body { width:100%; height:100%; } 
canvas { width: 95vw;  height: 250px;  } 
.label { text-align: center; margin-top: 5px; margin-bottom: 25px; }
.context { text-align: center; margin-top: 5px; margin-bottom: 15px; }
#charts_info { text-align: center; margin-top: 5px; margin-bottom: 15px; }
#charts { display: grid; justify-content: center; align-items: center; border: 1px solid white; }
</style>
)~";
PROGMEM const char HTML_CHARTS_BODY[] = R"~(
</head>
<body>
<div class="container">
    <div id="charts_info"></div>
    <div id="charts"></div>
</div>
</body>
</html>
)~";
// Draw charts scripts
PROGMEM const char HTML_CHARTS_SCRIPT[] = R"~(
function isNumeric(n) {     return !isNaN(parseFloat(n)) && isFinite(n); }
function roundToPIPScale(number){     return Math.round(number * 1000000) / 1000000 }

function drawLine(ctx, sourceX, sourceY, destnationX, destnationY, strokeStyle="rgba(0, 0, 0, 0.7)"){
    ctx.beginPath();
    ctx.moveTo(sourceX, sourceY);
    ctx.lineTo(destnationX, destnationY);
    ctx.strokeStyle = strokeStyle
    ctx.stroke();   
}
 
function loadData(csv, max=300) {
    var allTextLines = csv.split(/\r\n|\n/);
    var headers = allTextLines.shift().split('\t');
    if(headers.length < 2) return;
    var data = [];
    var info = [];
    headers.forEach((e, i) => {
        headers[i] = e.trim();
        data[ headers[i] ] = [];
    });
    var n = 0;
    var name = "";
    data['timestamp'] = [];
    allTextLines.reverse();
    while (allTextLines.length) {
        var line = allTextLines.shift()
        if(!line)  continue;
        if(n >= max) break;
        line = line.split('\t');
        if(line.length==1){ 
            name = line[0].split(":")[1]
            name = name.split("/").slice(-1)[0].split(".")[0]
            continue;
        }
        headers.forEach((e, i) => {
            if(!info[e]){
                info[e] = new Object();
                info[e].min = Number.POSITIVE_INFINITY;
                info[e].max = Number.NEGATIVE_INFINITY;
                info[e].avg = 0;
                info[e].chLen = Number.NEGATIVE_INFINITY;
            } 
            if(e=="DateTime"){
                data[e].push(line[i]);
                var t = new Date(line[i]).getTime()                
                data['timestamp'].push(t/1000)                
                if(t < info[e].min) info[e].min = t;
                if(t > info[e].max) info[e].max = t;
                if(line[i].length > info[e].chLen ) info[e].chLen  = (line[i]).length;
            }else if(isNumeric(line[i])){
                var v = parseFloat(line[i])
                data[e].push(v);
                if(v < info[e].min) { info[e].min = v; info[e].minT = n }
                if(v > info[e].max) {  info[e].max = v; info[e].maxT = n }
                info[e].avg +=v;
                var chLen = (''+v).length
                if(Number.isInteger(v)) chLen +=2;
                if(chLen > info[e].chLen) info[e].chLen  = chLen;
            }else{
                data[e].push(line[i]);
                if(line[i].length > info[e].chLen ) info[e].chLen  = (line[i]).length;
            }
        });
        n++;
    }
    headers.forEach((e) => {
        if(info[e].avg) info[e].avg = (info[e].avg / n).toFixed(1); 
    });
    info['DateTime'].min = (new Date(data['DateTime'][0]).setHours(0, 0, 0, 0) )  / 1000;
    info['DateTime'].max = (new Date(data['DateTime'][0]).setHours(24, 0, 0, 0) ) / 1000;

    return { "name": name, "data": data, "info": info }
}

const ignore = ['DateTime','timestamp'] //,'soil','salt','batPerc']
const include = ['temp']
const headersDict = { 'temp': 'Temperature', 'humid': 'Humiditty', 'pressr': 'Atmospheric pressure', 'lux': 'Luminosity',
                      'soil': 'Soil moisture', 'salt': 'Soil saltivity', 'batPerc': 'Battery (%)' }
                      
function createCtxs(id, chartData){
    var data = chartData.data;
    var ctxNames = []
    Object.keys(data).forEach(key => {
        if( !ignore.includes(key) ){
        //if( include.includes(key) ){
            var span = document.createElement('span');
            const ctxName = id + '.' + key + '_cv';
            ctxNames.push([ctxName, key]);
            span.innerHTML += '<div class="wrapper">'
            span.innerHTML += '<div class="context"><canvas id="'+ ctxName +'" width="1024px" height="250px"></canvas></div>'
            if( headersDict[key] ) var inf = "<b>" + headersDict[key] + "</b>" 
            else var inf = "<b>" + key + "</b>"
            inf += "&nbsp;&nbsp;<span style='font-size: smaller;'>"
            inf += "Min: <b>"+ chartData.info[key].min + "</b> at " + chartData.data['DateTime'][ chartData.info[key].minT ].split(" ")[1] + ",&nbsp;&nbsp;"
            inf += "Max: <b>"+ chartData.info[key].max + "</b> at " + chartData.data['DateTime'][ chartData.info[key].maxT ].split(" ")[1]
            inf += "&nbsp;&nbsp; Avg: <b>" + chartData.info[key].avg + "</b>" 
            //inf += " len: "+ chartData.info[key].chLen
            inf += '</span>'
            span.innerHTML += '<div class="label">'+ inf  +'</div>'                
            span.innerHTML += '</div>'
            document.getElementById('charts').insertBefore(span, null);                
            drawChart(ctxName, chartData, key);
        }
    });
    return ctxNames;
}

function drawChart(ctxName, chObj, key){
    const chH = 18; const chW = 6;
    var canvas = document.getElementById(ctxName);
    if(!canvas) return;
    var ctx = canvas.getContext('2d');
    const rectColor = 'rgba(110, 110, 110, 0.5)';    
    ctx.clearRect(0, 0, canvas.width, canvas.height);  

    drawLine(ctx, 0 ,0, 0, canvas.height, rectColor);
    drawLine(ctx, 0, 0, canvas.width, 0, rectColor);
    drawLine(ctx, canvas.width, 0, canvas.width, canvas.height, rectColor);
    drawLine(ctx, 0, canvas.height, canvas.width, canvas.height, rectColor);
    
    if(chObj.info[key].chLen < 3 ) chObj.info[key].chLen = 4;
    
    var cw = canvas.width - (chW * chObj.info[key].chLen)- 2;
    var ch = canvas.height - 1 * chH;
    
    const dY = ((chObj.info[key].max - chObj.info[key].min) / 100) * 90;
    const multY = ((ch / dY) / 100) * 90;  

    const dX = chObj.info['DateTime'].max - chObj.info['DateTime'].min;
    const multX = ((cw / dX) / 100) * 100;  

    var lastY = 0;
    var lastX = 0;
    for(var i = 0; i < chObj.data[key].length;i++){
        const curY = ((chObj.data[key][i] * multY) * -1) + (chObj.info[key].min * multY) + ch ;        
        const curX = (chObj.data['timestamp'][i] - chObj.info['DateTime'].min) * multX;
        if(i == 0){ lastY = curY; lastX = curX; }
        drawLine(ctx, lastX, lastY, curX, curY)
        ctx.beginPath();
        ctx.arc(curX, curY, 2, 0, 2 * Math.PI);
        ctx.strokeStyle = 'rgba(110, 110, 110, 0.5)';
        ctx.stroke();
        lastY = curY; lastX = curX;
    } 
    
    const labelCountY =  ch / chH
    const stepSizeY =  ch / labelCountY;
    console.log(labelCountY, stepSizeY )
    for(var i = 0; i <= labelCountY; i++){             
        var currentScale = (1 / labelCountY) * i;             
        var label = roundToPIPScale(chObj.info[key].min + (dY * currentScale)).toFixed(1);
        const y = ((stepSizeY * i) * -1 ) + ch 
        ctx.fillText( label, cw + 3, y ) ; 
        drawLine(ctx, 0, y, cw + 0, y, 'rgba(110, 110, 110, 0.3)');
    }  
    
    const labelCountX = 24
    const stepSizeX = cw / labelCountX;
    for(var i = 0; i <= labelCountX; i++){
        var label = 24 - i + ":00"        
        var ld = 5 * label.length;
        const x = ((stepSizeX * i) * -1 ) + cw 
        var xl = x - ld / 2
        if(xl < 0) xl = 2;
        ctx.fillText( label, xl, ch + 12 ) ; 
        drawLine(ctx, x, 0, x, ch, 'rgba(200, 200, 200, 0.3)');
    } 
}
)~";

// Chart page scripts
PROGMEM const char HTML_CHARTS_MAIN_SCRIPT[] = R"~(
    document.addEventListener('DOMContentLoaded', function (event) {
        const $ = document.querySelector.bind(document);
        const $$ = document.querySelectorAll.bind(document);            
        const refreshInterval = 30000
        var drawTimer=0;
        let contexts = []
        
        async function plotCurLog() {      
            const baseHost = document.location.origin;
            const url = baseHost + "/cmd?view="
            try {
                const response = await fetch(encodeURI(url));
                var txt = await response.text();
                if (!response.ok){        
                    console.log('Error:', txt)
                }else{
                    var js = loadData( txt ,450);
                    $('#charts_info').innerHTML = js.name
                    $('#charts').innerHTML = "";
                    contexts = [];
                    contexts.push(createCtxs(js.name, js))
                }
            } catch(e) {
                console.log(e,url)
            }
            clearTimeout(drawTimer);
            drawTimer = setTimeout(plotCurLog, refreshInterval); 
        }

        plotCurLog()
    });  
)~";
