var client = null;

function uploadfiles() {
    var filebox = document.getElementById("filebox");
    filebox.onchange = function (e)
    {
        var files = filebox.files;
        if (!files || (files.length < 1)) {
            document.getElementById("status").innerHTML = "Keine Dateien ausgewählt";        
            return;
        }
        var formData = new FormData();
        for (var i = 0; i < files.length; i++) {
            formData.append(files[i].name, files[i]);
        }
        var request = new XMLHttpRequest();
        request.onreadystatechange = function (e) {
            if (this.readyState == this.DONE) {
                document.getElementById("status").innerHTML = this.status == 200 ? "Hochgeladen": "Fehler";
            }
        }
        request.upload.onprogress = function (e) {
            document.getElementById("progressbar").value = e.loaded / e.total;
            document.getElementById("status").innerHTML = e.loaded + "/" + e.total + "Bytes";
        }
        var path = document.getElementById("path").innerHTML;
        request.open("POST", "/libordner.so?folder=" + (path != "" ? path : "/"));
        request.send(formData);
    }
    filebox.click();
}

function uploadabort() {
    if (client != null) {
        client.abort();
        document.getElementById("status").innerHTML = "Abgebrochen";
    }
}