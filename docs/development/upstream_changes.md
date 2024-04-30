# How to Include Upstream `xk` Updates

<!-- Originally by: tenzinhl -->

## Intro

Occasionally the staff may make updates to lab handouts, code, etc. to improve the lab experience (or just flat-out fix errors on our end, although you didn't hear that from me).

This document is meant to serve as a quick guide on how to incorporate upstream changes (i.e.: changes made to our public version of the repo) when we announce them!

## How-to

Run the following commands inside your local copy of the repo:

1. `git checkout main` (Let's make sure you're on your own main branch, or checkout whatever branch you're looking to merge upstream/main into).
2. `git fetch upstream` to fetch updates to the upstream remote (which you should've configured as part of lab1 setup to point to the staff's public version of xk base code).
3. `git merge upstream/main`

Voila! And with that you should have the updates in your main branch (or whichever branch you chose).
