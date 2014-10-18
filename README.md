# similine

This program examines a file line-by-line and shows pairs of lines that are similar. The similarity threshold is configurable.

## Usage

```
similine [files]...

-s, --stop-after=number        maximum number of lines to process
-t, --threshold=double         similarity threshold (default 0.75)
-w, --whitespace-sensitive     use whitespace-sensitive comparison (do not collapse whitespace)
-c, --case-sensitive           use case-sensitive comparison
-b, --show-blank-lines         show comparisons with blank lines

-?, --help                     Show this help message
    --usage                    Display brief usage message
```

## Algorithm

There are several common algorithms for measuring similarity of text. I briefly experimented with Levenstein distance and longest common subsequence, but seemed to get the best results from the Sørensen–Dice coefficient (https://en.wikipedia.org/wiki/Sørensen–Dice_coefficient). The implemented algorithm is a variant that accounts for case and whitespace sensitivity.

## Dependencies

libpopt. Tested only with glibc on GNU/Linux.
