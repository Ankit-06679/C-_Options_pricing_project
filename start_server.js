var shell = new ActiveXObject("WScript.Shell");
var env = "Path=" + shell.ExpandEnvironmentStrings("%Path%") + ";C:\\msys64\\mingw64\\bin";
shell.Environment("PROCESS")("Path") = env;
shell.Run("C:\\Users\\Ankit\\AppData\\Local\\Temp\\opencode\\pricing-build2\\options_pricer.exe U5MKWZ7B34ZLO2GS AAPL TSLA --port 8081", 0, false);
