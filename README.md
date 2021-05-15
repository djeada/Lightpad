<div align="center">
<a href="https://github.com/djeada/Lightpad/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/djeada/Lightpad"></a>
<a href="https://github.com/djeada/Lightpad/network"><img alt="GitHub forks" src="https://img.shields.io/github/forks/djeada/Lightpad"></a>
<a href="https://github.com/djeada/Lightpad/blob/master/LICENSE.txt"><img alt="GitHub license" src="https://img.shields.io/github/license/djeada/Lightpad"></a>
<a href=""><img src="https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat"></a>
<a href=""><img src="https://img.shields.io/badge/version-0.1--beta-brightgreen"></a>
<a href=""><img src="https://img.shields.io/badge/-beta-orange"></a>
</div>

![Lightpad](https://github.com/djeada/Lightpad/blob/master/App/resources/icons/app.png)

# Lightpad
Open source code editor, developed with Qt framework.

<h1>Screenshot</h1>

![Alt text](https://github.com/djeada/Lightpad/blob/master/screenshot.png)

<h1>Features</h1>

<ul>
  <li>Search and replace</li>
  <li>Syntax highlighting</li>
  <li>Editing shortcuts</li>
  <li>Color themes</li>
  <li>Code templates</li>
  <li>Auto parentheses</li>
  <li>Auto indenting</li>
</ul>

<h2>Features to be added</h2>
<ul>
  <li>Full VIM compatibility </li>
  <li>Auto completion</li>
</ul>

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
Open the terminal in app directory and write: 
<br>
<pre>qmake Lightpad.pro
make</pre>

<h2>Run</h2>
To run Lightpad, navigate to the program directory and double-click Lightpad icon.

If you had problems compiling from source, raise a new issue in the <a href = https://github.com/djeada/lightpad/issues> link</href>.

<h1>Contributing </h1>
All contributions are welcomed.

<h1>License</h1>
This project is licensed under  GNU GENERAL PUBLIC LICENSE - see the <a href='https://github.com/djeada/Lightpad/blob/master/LICENSE.txt'> LICENSE.txt </a> file for details.
