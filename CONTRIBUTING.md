## Contributing to uvgRTP

When issuing a pull request (PR) to uvgRTP. Please consider the following aspects for easier and faster merging of PR. It is recommended to first create an issue to coordinate the the creation of PR, but this is not a requirement.

### Code Style
- Tabs are not allowed anywhere because they may be rendered differently on different platforms
- Try to follow the style of existing code including indentation
- Try limiting line length to 100 characters, function size to 100 lines and file size to 1000 lines

### PR code
- Should compile both on with newest GCC, MinGW and MSVC. We can help with testing if needed.
- Try to avoid code duplication

### Version history
- Try to keep commits small and limited to improving one specific aspect of uvgRTP
- Try to limit the PR to fix one specific issue in uvgRTP
- Don't merge to master, the PR will be merged automatically when accepted. If your PR gets outdated, rebase the branch instead.
- If you need to change the PR, use force push instead of adding new commits to the branch. The force pushing the PR branch also updates the Github PR.

### Commit format
uvgRTP follows a specific commit format. The commit header begins with a prefix. List of current usd prefixes can be found in [here](docs/subsystems). The "common:"-prefix is used for modules not found in the list and for changes that affect more than one module. Try to limit line length of commit message to 72 character for better readability in different environments.

Example commit:
```
subsystem: A concice description of commit

This text describes what this commit does or fixes, how it works on 
a high level and possibly why this implementation was chosen over other
possibilities. You may also add the Github issue number, but please make
sure the commit message is understandable without reading the issue.
```

### Submitting a PR

We will do the best we can to help your PR to meet the standards of uvgRTP. Don't be afraid to submit a PR even if you are sure it meets these criteria. We try to respond to PRs in a timely fashion during weekdays unless everyone is busy or on holidays.

