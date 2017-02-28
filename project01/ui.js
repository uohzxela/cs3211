
   var loadedImage = false;
   var arr = [];
   var isGpuEnabled = false;
   var isEdgeEnabled = false;
   var isSharpenEnabled = false;
   var isBlur3x3Enabled = false;
   var isBlur5x5Enabled = false;
   var isBlur7x7Enabled = false;
   var isBlur9x9Enabled = false;
   var isOptimized = false;
   var animation = true;
   var firsttime = true;

   function toggleMode( el ) {
      isGpuEnabled = !isGpuEnabled;
      el.value = "Using " + (isGpuEnabled ? "GPU" : "CPU");
   }
   function toggleOptimizations(el) {
      isOptimized = !isOptimized;
      el.value = "Blur optimization " + (isOptimized ? "on" : "off") + " (for non-pipeline use)";
   }
   function toggleEdge( el ) {
      isEdgeEnabled = !isEdgeEnabled;
      if (isEdgeEnabled) {
         el.value = "Edge detection on";
         pipeline.push("edge");
      } else {
         el.value = "Edge detection off";
         var idx = pipeline.indexOf("edge");
         if (idx > -1) {
            pipeline.splice(idx, 1);
         }
      }
   }

   function toggleSharpen( el ) {
      isSharpenEnabled = !isSharpenEnabled;
      if (isSharpenEnabled) {
         el.value = "Sharpen on";
         pipeline.push("sharpen");
      } else {
         el.value = "Sharpen off";
         var idx = pipeline.indexOf("sharpen");
         if (idx > -1) {
            pipeline.splice(idx, 1);
         }
      }
   }

   function toggleBlur3x3( el ) {
      isBlur3x3Enabled = !isBlur3x3Enabled;
      if (isBlur3x3Enabled) {
         el.value = "Blur 3x3 on";
         pipeline.push("blur3x3");
      } else {
         el.value = "Blur 3x3 off";
         var idx = pipeline.indexOf("blur3x3");
         if (idx > -1) {
            pipeline.splice(idx, 1);
         }
      }
   }

   function toggleBlur5x5( el ) {
      isBlur5x5Enabled = !isBlur5x5Enabled;
      if (isBlur5x5Enabled) {
         el.value = "Blur 5x5 on";
         pipeline.push("blur5x5");
      } else {
         el.value = "Blur 5x5 off";
         var idx = pipeline.indexOf("blur5x5");
         if (idx > -1) {
            pipeline.splice(idx, 1);
         }
      }
   }

   function toggleBlur7x7( el ) {
      isBlur7x7Enabled = !isBlur7x7Enabled;
      if (isBlur7x7Enabled) {
         el.value = "Blur 7x7 on";
         pipeline.push("blur7x7");
      } else {
         el.value = "Blur 7x7 off";
         var idx = pipeline.indexOf("blur7x7");
         if (idx > -1) {
            pipeline.splice(idx, 1);
         }
      }
   }

   function toggleBlur9x9( el ) {
      isBlur9x9Enabled = !isBlur9x9Enabled;
      if (isBlur9x9Enabled) {
         el.value = "Blur 9x9 on";
         pipeline.push("blur9x9");
      } else {
         el.value = "Blur 9x9 off";
         var idx = pipeline.indexOf("blur9x9");
         if (idx > -1) {
            pipeline.splice(idx, 1);
         }
      }
   }
   function changeAnimation( el ) {
      if ( el.value === "Animation" ) {
	animation = false;
	el.value = "No Animation";
    } else {
	animation = true;
	el.value = "Animation";
    }
}

function loadImage() {
   var canvas = document.getElementById("backimageCanvas");
   var ctx = canvas.getContext('2d');
   var imag = document.getElementById("backimage");
   ctx.drawImage(imag, 0, 0);
   imag.style.display = 'none';

   var imageData = ctx.getImageData(0, 0, 800, 600);

   for (var channel=0; channel<4; channel++) {
      arr.push([]);
      for (var y=0; y<600; y++) {
         arr[channel].push([]);
      }
   }
   var pointer = 0;
   for (var y=0; y<600; y++) {
      for (var x=0; x<800; x++) {
         arr[0][600-y-1][x] = imageData.data[pointer++]/256;
         arr[1][600-y-1][x] = imageData.data[pointer++]/256;
         arr[2][600-y-1][x] = imageData.data[pointer++]/256;
         arr[3][600-y-1][x] = imageData.data[pointer++]/256;
      }
   }
   loadedImage = true;
}

