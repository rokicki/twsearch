Module = {} ;
Module.print = function(text) {
   self.postMessage(['stdout', text]) ;
} ;
Module.printErr = Module.print ;
var stdinData = [] ;
readline = function() {
   if (stdinData.length == 0)
      return null ;
   return stdinData.shift() ;
}
Module.noInitialRun = true ;
Module.preRun = [] ;
importScripts("twsearch.js")
function gotMessage(e) {
   e = e.data ;
   var cmd = e[0] ;
   if (cmd == 'createfile') {
      Module.preRun.push(function() {
         FS.createDataFile("/", e[1], e[2], true, true) ;
      }) ;
   } else if (cmd == 'start') {
      var args = [...e] ;
      args.shift(); 
      shouldRunNow = true ;
      calledRun = false ;
      arguments_ = args ; // in case emscripten isn't ready
      run(args) ;
   } else if (cmd == 'stdin') {
      stdinData = [...e] ;
      stdinData.shift() ;
   } else {
      self.postMessage(e.data) ;
   }
}
self.addEventListener('message', gotMessage);
