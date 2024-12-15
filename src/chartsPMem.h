PROGMEM const char HTML_CHARTS_CSS[] = R"~(
<style>
body { margin: 2px; padding:2px; }
html, body { width:100%; height:100%; }
canvas { width: 95vw;  height: 250px;  }
.label { text-align: center; margin-top: 5px; margin-bottom: 0px; }
.context { text-align: center; margin-top: 5px; margin-bottom: 10px; }
#charts_info { text-align: center; margin-top: 10px; margin-bottom: 10px; }
#charts { display: grid; justify-content: center; align-items: center; border: 1px solid white; }
</style>
)~";
PROGMEM const char HTML_CHARTS_BODY[] = R"~(
</head>
<body>
<div class="container">
    <div id="chart_sel" style="text-align: center;">
        <button type="button" onclick="changeGraph(+1)"><<</button>
        <select id="charts_list" name="charts"></select>
        <button type="button" onclick="changeGraph(-1)">>></button>
        &nbsp;
        <button type="button" onclick="changeMonth(+1)"><<</button>
        <select id="months_list" name="months"></select>
        <button type="button" onclick="changeMonth(-1)">>></button>
    </div>
    <div id="charts"></div>
    <div id="charts_info"></div>
</div>
<div style="text-align: center;">
<button type="button" onClick="window.location.href='/'; return false;">Back</button>
</div>
<br>
</body>
</html>
)~";
// Draw charts scripts
PROGMEM const char HTML_CHARTS_SCRIPT[] = R"~(
function isNumeric(n) {     return !isNaN(parseFloat(n)) && isFinite(n); }
function roundToPIPScale(number){     return Math.round(number * 1000000) / 1000000 }

function drawLine(ctx, sourceX, sourceY, destnationX, destnationY, strokeStyle="rgba(0, 0, 0, 0.5)", dash=[]){
    ctx.beginPath();
    ctx.moveTo(sourceX, sourceY);
    ctx.lineTo(destnationX, destnationY);
    ctx.strokeStyle = strokeStyle
    ctx.setLineDash(dash);
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
                info[e].minT = 0;
                info[e].maxT = 0;
                info[e].avg = 0;
                info[e].chLen = Number.NEGATIVE_INFINITY;
            }
            if(e=="DateTime"){
                data[e].push(line[i]);
                var t = new Date(line[i]).getTime()
                data['timestamp'].push(t)
                if(t < info[e].min) info[e].min = t;
                if(t > info[e].max) info[e].max = t;
                //if(line[i].length > info[e].chLen ) info[e].chLen  = (line[i]).length;
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
        if(info[e].min == Number.POSITIVE_INFINITY) info[e].min = 0
        if(info[e].max == Number.NEGATIVE_INFINITY) info[e].max = 0
    });
    info['DateTime'].chLen = 5;
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
            if( headersDict[key] ) var inf = "<b>" + headersDict[key] + "</b>"
            else var inf = "<b>" + key + "</b>"
            span.innerHTML += '<div class="wrapper">'
            inf += "&nbsp;&nbsp;<span style='font-size: smaller;'>"
            inf += "Min: <b>"+ chartData.info[key].min + "</b> at " + chartData.data['DateTime'][ chartData.info[key].minT ].split(" ")[1] + ",&nbsp;&nbsp;"
            inf += "Max: <b>"+ chartData.info[key].max + "</b> at " + chartData.data['DateTime'][ chartData.info[key].maxT ].split(" ")[1]
            inf += "&nbsp;&nbsp; Avg: <b>" + chartData.info[key].avg + "</b>"
            span.innerHTML += '<div class="label">'+ inf  +'</div>'
            span.innerHTML += '<div class="context"><canvas id="'+ ctxName +'" width="1024px" height="250px"></canvas></div>'
            //inf += " len: "+ chartData.info[key].chLen
            inf += '</span>'
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
    const rectColor = 'rgba(100, 100, 100, 1)';
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    drawLine(ctx, 0 ,0, 0, canvas.height, rectColor);
    drawLine(ctx, 0, 0, canvas.width, 0, rectColor);
    drawLine(ctx, canvas.width, 0, canvas.width, canvas.height, rectColor);
    drawLine(ctx, 0, canvas.height, canvas.width, canvas.height, rectColor);

    var cw = canvas.width - (chW * chObj.info[key].chLen) - 2;
    var ch = canvas.height - 2*chH;

    const dY = (chObj.info[key].max  - chObj.info[key].min);
    const multY = (ch / dY);

    const dX = chObj.info['DateTime'].max - chObj.info['DateTime'].min;
    const multX = (cw / dX);
    var lastY = 0;
    var lastX = 0;
    for(var i = 0; i < chObj.data[key].length;i++){
        const curY = ((chObj.data[key][i] * multY) * -1) + (chObj.info[key].min * multY) + ch + chH;
        const curX = (chObj.data['timestamp'][i] - chObj.info['DateTime'].min) * multX;
        if(i == 0){ lastY = curY; lastX = curX; }
        drawLine(ctx, lastX, lastY, curX, curY)
        ctx.beginPath();
        ctx.arc(curX, curY, 2, 0, 2 * Math.PI);
        ctx.strokeStyle = 'rgba(110, 110, 110, 0.3)';
        ctx.stroke();
        lastY = curY; lastX = curX;
    }

    const labelCountY =  Math.ceil((ch / chH) + 0)
    const stepSizeY =  ch / labelCountY;
    var label = ""
    for(var i = 0; i <= labelCountY; i++){
        var currentScale = (1 / labelCountY) * i;
        label = roundToPIPScale(chObj.info[key].min + (dY * currentScale)).toFixed(1);
        const y = ((stepSizeY * i) * -1 ) + ch + chH
        ctx.fillText( label, cw + 3, y ) ;
        drawLine(ctx, 0, y, cw + 0, y, 'rgba(110, 110, 110, 0.3)');
    }

    const labelCountX = 24
    const stepSizeX = cw / labelCountX;
    for(var i = 0; i <= labelCountX; i++){
        label = 24 - i + ":00"
        var ld = 5 * label.length;
        const x = ((stepSizeX * i) * -1 ) + cw
        var xl = x - ld / 2
        if(xl < 0) xl = 2;
        ctx.fillText( label, xl, ch + chH + 14 ) ;
        drawLine(ctx, x, 0, x, ch + chH, 'rgba(200, 200, 200, 0.3)');
    }
}
)~";

// Chart page scripts
PROGMEM const char HTML_CHARTS_MAIN_SCRIPT[] = R"~(
const $ = document.querySelector.bind(document);
const $$ = document.querySelectorAll.bind(document);
const refreshInterval = 30000
//var drawTimer=0;
let contexts = []

async function changeGraph(i){
    const list = $('#charts_list')
    if(this.id=='charts_list'){
        file = this.value
    }else{
        var n = list.selectedIndex + i;
        if(n < 0){
            const r = await changeMonth(-1);
            if(!r) return
            console.log(list)
            n = list.options.length - 1;
        }
        if(n >= list.options.length){
            const r = await changeMonth(+1);
            if(!r) return
            n = 0
        }
        file = list.options[n].value
        list.selectedIndex = n;
    }
    const r = await drawLog(file);
}

async function changeMonth(i){
    const list = $('#months_list')
    if(this.id=='months_list'){
        file = this.value
    }else{
        var n = list.selectedIndex + i;
        if(n < 0 || n >= list.options.length ) return false;
        file = list.options[n].value
        list.selectedIndex = n;
    }
    $('#charts_list').innerHTML="";
    const res = await listFiles("/data/" + file)
    changeGraph(0)
    return true;
}

async function listFiles(dir="") {
    var baseHost = document.location.origin;
    var url = baseHost + "/cmd?ls="
    if(dir!="")  url += "&dir="+dir
    try {
        const response = await fetch(encodeURI(url));
        var txt = await response.text();
        if (false && !response.ok){
            console.log('Error:', txt)
        }else{
            var lines = txt.split(/\r\n|\n/);
            while (lines.length) {
                var line = lines.shift()
                if(line=="" || lines.length == 1) continue;
                if(dir==""){
                    if(line.startsWith("[") )
                        $('#months_list').add(new Option(line.replace("[","").replace("]","").replace("/data/","")));
                }else{
                    if(line.startsWith("[") ) continue;
                    var opt = line.split("\t")[0]
                    var optT = opt.split("-").slice(-1)[0].split(".")[0]
                    if(opt.length>1){
                        $('#charts_list').add(new Option(optT,opt) );
                    }

                }
            }
            console.log("List end:", dir)
            return response;
        }
    } catch(e) {
        console.log(e,url)
    }

}
async function getLog(name) {
    var baseHost = document.location.origin;
    const url = baseHost + "/cmd?view="+name;
    try {
        const response = await fetch(encodeURI(url));
        var txt = await response.text();
        if (!response.ok){
            console.log('Error:', txt)
        }else{
            return txt;
        }
    } catch(e) {
        console.log(e,url)
    }
    return ""
}
async function drawLog(name) {
    var txt = await getLog(name)
    try {
        var js = loadData( txt ,450);
        js.info['DateTime'].min = (new Date(js.data['DateTime'][0]).setHours(0, 0, 0, 0) ) ;
        js.info['DateTime'].max = (new Date(js.data['DateTime'][0]).setHours(24, 0, 0, 0) ) ;
        $('#charts_info').innerHTML = "<span style='font-size: smaller;'>";
        $('#charts_info').innerHTML += "File: <b>" + name + "</b>"
        $('#charts_info').innerHTML += ", Points: <b>" + js.data['DateTime'].length + "</b>"
        $('#charts_info').innerHTML += ", Start: <b>" + js.data['DateTime'].slice(-1)[0]  + "</b>"
        $('#charts_info').innerHTML += ", End: <b>" + js.data['DateTime'][0] + "</b>"
        $('#charts').innerHTML = "";
        contexts = [];
        contexts.push(createCtxs(js.name, js))
    } catch(e) {
        console.log(e, name)
    }
}

async function Init() {
  //console.log('Init');
  const r1 = await listFiles()
  const r2 = await changeMonth(0)
}
document.addEventListener('DOMContentLoaded', function (event) {
    $('#months_list').addEventListener("change", changeMonth);
    $('#charts_list').addEventListener("change", changeGraph);
    Init();
});
)~";
