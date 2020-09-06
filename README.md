# Lightpad
Open source code editor, developed with Qt framework.

<h1>Features</h1>

<ol>
  <li>Searching</li>
  <li>Syntax highlighting</li>
  <li>Editing shortcuts</li>
  <li>Color themes</li>
  <li>Auto parentheses</li>
  <li>Code templates</li>
</ol>

<h2>Features to be added</h2>
<ol>
  <li>Full VIM compatibility </li>
  <li>Auto-completion</li>
</ol>

<h1>Build</h1>
To build Lightpad, you need some libraries and tools.  Follow those instructions for the platform you're compiling on.

<h2>Get the sources</h2>
Clone the Lightpad repository:
<br>
<pre>git clone https://github.com/djeada/Lightpad.git</pre>

<h2>Install Build-Tools</h2>

<ul>  
<li> A C++ compiler supporting C++14 </li>
<li> Cmake </li>
</ul>
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

If you had problems compiling from source, raise a new issue in the <a href = https://github.com/djeada/lightpad/issues> link</href>.

<h1>Contributing </h1>
All contributions are welcomed.

<h1>License</h1>
This project is licensed under  GNU GENERAL PUBLIC LICENSE - see the <a href='https://github.com/djeada/Lightpad/blob/master/LICENSE.txt'> LICENSE.txt </a> file for details.
