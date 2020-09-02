# Lightpad
Open source code editor, developed with Qt framework.

<h1>Build</h1>

Follow those instructions.

<h2>Get the sources</h2>
Clone the Lightpad repository: </br>

<code>git clone https://github.com/djeada/Lightpad.git</code>

Install Build-Tools
<code>sudo apt-get install build-essential</code>

Install dependencies
<code>sudo apt-get install libqt5webkit5-dev qttools5-dev-tools qt5-default \
                     discount libmarkdown2-dev libhunspell-dev</code>
                     
<h2>Building with Qmake</h2>
Open the terminal in app direcory and write: </br>

<code>qmake Lightpad.pro</code>
<code>make</code>

<h2>Run</h2>
Go to app directory and double click Lightpad to run.
