Any and all contributions are appreciated.

* You can fork this repository by pressing the fork button at the top of the page.  This will duplicate the repository in your account.

* You can now clone the forked archive on your local machine.

  - Run the following git command in a terminal:

```bash
git clone https://github.com/djeada/Lightpad.git
```

* Navigate to repository directory on your computer.

  - Now use the git checkout command to create a new branch:

```bash
git checkout -b your-new-branch-name
```
 - If you get an error, you may need to fetch your-new-branch-name first.

```bash
git remote update && git fetch
```

* Once you've made the updates, push them to the repository.

  - Please describe your change in a commit message.

```bash
git add file
git commit -m "your commit message"
git push origin <your-new-branch-name>
```

* Now go to the github website and make a <i>Pull Request</in> in your forked repository.
  - Make sure that your <i>Pull Request</in> was for the branch.
