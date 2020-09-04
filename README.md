# Lightpad
Open source code editor, developed with Qt framework.

<h1>Build</h1>
Follow those instructions.

<h2>Get the sources</h2>
Clone the Lightpad repository:
<br>
<pre>git clone https://github.com/djeada/Lightpad.git</pre>

<h2>Install Build-Tools</h2>
<pre>sudo apt-get install build-essential</pre>

<h2>Install dependencies</h2>
<pre>sudo apt-get install libqt5webkit5-dev qttools5-dev-tools qt5-default</pre>
                     
<h2>Building with Cmake</h2>
Open the terminal in app direcory and write: 
<pre>mkdir build
cd build
cmake -GNinja ..
ninja ../Lightpad</pre>

<h2>Building with Qmake</h2>
Open the terminal in app direcory and write: 
<br>
<pre>qmake Lightpad.pro
make</pre>

<h2>Run</h2>
Go to app directory and double click Lightpad to run.
