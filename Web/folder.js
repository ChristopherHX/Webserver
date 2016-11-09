var client = null;

function uploadfiles() {
    var files = document.getElementById("filebox").files;
    if (!files) {
        return;
    }
    var formData = new FormData();
    for (var i = 0; i < files.length; i++) {
        formData.append(files[i].name, files[i]);
    }
    var request = new XMLHttpRequest();
    var iframe = document.getElementById("iframe");
    request.onreadystatechange = function (e) {
        if (this.readyState == this.DONE) {
            document.getElementById("status").innerHTML = this.status == 200 ? "Hochgeladen": "Fehler";
            iframe.contentDocument.location.reload(true);
        }
    }
    request.upload.onprogress = function (e) {
        document.getElementById("progressbar").value = e.loaded / e.total;
        document.getElementById("status").innerHTML = e.loaded + "/" + e.total + "Bytes";
    }
    var path = iframe.contentDocument.getElementById("path").innerHTML;
    request.open("POST", "/uploadfiles?" + (path != "" ? path : "/"));
    request.send(formData);
}

function uploadabort() {
    if (client != null) {
        client.abort();
        document.getElementById("status").innerHTML = "Abgebrochen";
    }
}