<h1 align="center">Wind Turbine SCADA Data Analysis (Mini)</h1>
<h3 align="center">using C++ (Arrays, Linked Lists &amp; Stack)</h3>

<p align="center">
A college mini-project for processing real turbine sensor telemetry —
detecting faults, and letting a user query, filter, sort, and search
telemetry data by column, entirely through a terminal interface.
</p>

<p align="center">
<img src="https://img.shields.io/badge/Language-C%2B%2B-blue">
<img src="https://img.shields.io/badge/Data-Real%20SCADA-green">
<img src="https://img.shields.io/badge/Interface-Terminal%20Only-lightgrey">
<img src="https://img.shields.io/badge/Status-In%20Development-yellow">
</p>

<hr>

<h2>Overview</h2>

<table>
<tr><td><b>Domain</b></td><td>Green Energy — Small Wind Turbine Analytics</td></tr>
<tr><td><b>Core Language</b></td><td>C++ (Arrays, Linked Lists, Stack)</td></tr>
<tr><td><b>Primary Dataset</b></td><td><code>swt_august_2022_week_16_22.csv</code> (9,614 real telemetry records, 1-minute resolution)</td></tr>
<tr><td><b>Data Type</b></td><td>Actual SCADA Data</td></tr>
<tr><td><b>Interface</b></td><td>Terminal only — no GUI/dashboard</td></tr>
<tr><td><b>Level</b></td><td>College Mini-Project</td></tr>
</table>

<p>
This project processes real turbine sensor readings — wind speed, RPM,
DC/AC voltages, current, power output, temperatures, and bitmask status
codes — to detect fault conditions and let a user interactively explore
the data by any column, entirely from the terminal.
</p>

<hr>

<h2>Dataset</h2>

<p>
<code>swt_august_2022_week_16_22.csv</code> — a 9,614-record, one-week
subset (August 16–22, 2022) of real Skystream 3.7 SCADA telemetry,
trimmed from a larger source dataset. The week was chosen because it
contains the densest concentration of Rapid Change fault events in the
dataset, ensuring the fault detection and flood clustering logic has
meaningful data to work with.
</p>

<p><b>Source dataset:</b></p>
<blockquote>
Bassi, W., Rodrigues, A. L., &amp; Sauer, I. L. (2023). <i>Operation
SCADA Data of an Urban Small Wind Turbine in São Paulo, Brazil</i>
[Data set]. Zenodo.
<a href="https://doi.org/10.5281/zenodo.7348454">https://doi.org/10.5281/zenodo.7348454</a>.
Licensed under
<a href="https://creativecommons.org/licenses/by/4.0/">CC-BY-4.0</a>.
Recorded from a Southwest Windpower / XZERES <b>Skystream 3.7</b>
installed at the Institute of Energy and Environment, University of
São Paulo.
</blockquote>

<hr>

<h2>Files Present</h2>

<table>
<tr><th>File</th><th>Description</th></tr>
<tr><td><code>ingestion.cpp</code></td><td>Loads and parses CSV telemetry into structured records; implements the 4 fault detection rules</td></tr>
<tr><td><code>column_report.cpp</code></td><td>Interactive column-based filter/sort/search module with stack-based navigation</td></tr>
<tr><td><code>swt_august_2022_week_16_22.csv</code></td><td>Trimmed dataset (see above)</td></tr>
<tr><td><code>column_report_design.md</code> / <code>.docx</code></td><td>Design document for the Column Report module</td></tr>
</table>

<hr>

<h2>Data Structures Used</h2>

<table>
<tr><th>Structure</th><th>Where</th><th>Why</th></tr>
<tr>
<td><b>Array</b> (<code>vector&lt;TurbineRecord&gt;</code>)</td>
<td>Telemetry storage</td>
<td>Indexed access needed for filtering, sorting, and searching</td>
</tr>
<tr>
<td><b>Array-based Stack</b> (self-implemented)</td>
<td>Column Report navigation history</td>
<td>LIFO undo/back-navigation; operations are replayed from the master array rather than cached, with explicit overflow handling</td>
</tr>
<tr>
<td><b>Linked List</b> <i>(planned)</i></td>
<td>Fault flood clustering</td>
<td>One list per fault rule, grouping repeated triggers into clusters</td>
</tr>
<tr>
<td><code>enum</code></td>
<td>Column types, operation types</td>
<td>Named, fixed categories instead of raw integers</td>
</tr>
</table>

<hr>

<h2>Modules Implemented So Far</h2>

<h3>1. Ingestion (<code>ingestion.cpp</code>)</h3>
<ul>
<li>Parses all 17 telemetry columns from the CSV into a <code>TurbineRecord</code> struct</li>
<li>Validates and skips corrupt rows</li>
<li>Implements 4 threshold-based fault detection rules, empirically derived and verified against the dataset:</li>
</ul>

<table>
<tr><th>Rule</th><th>Condition</th><th>Candidates (in this dataset)</th></tr>
<tr><td>Wind-Power Mismatch</td><td>Windspeed ≥ 4 m/s, Power out &lt; 50W, turbine not already idle/braking</td><td>15</td></tr>
<tr><td>Temp-Without-Load</td><td>Power out &lt; 10W, (T1 − T3) delta &gt; 5°C</td><td>22</td></tr>
<tr><td>Tip-Speed Ratio Anomaly</td><td>Windspeed ≥ 3 m/s, RPM ÷ Windspeed outside 20–55</td><td>3</td></tr>
<tr><td>Rapid Change Detection</td><td>Absolute change in Power out between consecutive readings &gt; 500W</td><td>384</td></tr>
</table>

<h3>2. Column Report (<code>column_report.cpp</code>)</h3>
<p>Lets the user pick any of the 17 columns and run type-appropriate operations on it:</p>

<table>
<tr><th>Column Type</th><th>Filter</th><th>Sort</th><th>Search</th></tr>
<tr><td>Numeric / Counter</td><td>Range, Threshold</td><td>Ascending / Descending</td><td>Nearest-value</td></tr>
<tr><td>Bitmask (status codes)</td><td>Exact match, Bit-is-set</td><td>By code</td><td>Exact match</td></tr>
<tr><td>Timestamp</td><td>Date range, Time-of-day range</td><td>Chronological</td><td>—</td></tr>
</table>

<p><b>Key design points:</b></p>
<ul>
<li>Operations chain across multiple columns (AND logic) via a <b>"Change Column"</b> option that preserves current results</li>
<li>Navigation (<b>Back / Change Column / Main Menu</b>) is handled with a self-implemented array-based stack — "Back" rebuilds results by <b>replaying</b> the remaining operation history from the master dataset, rather than caching intermediate snapshots</li>
<li>Column-specific info (current range, status codes present, or date span) is shown before each prompt, so the user isn't guessing valid input values</li>
<li>All filtering, sorting, and searching is self-implemented — no STL algorithms — to demonstrate the underlying data structures and algorithms directly</li>
</ul>

<hr>

<h2>Standards Reference</h2>

<p>
This project's planned fault-clustering design is based on
<b>ANSI/ISA-18.2-2016, Management of Alarm Systems for the Process
Industries</b> — verified as the current core version of the standard
as of 2026 (first published 2009, revised 2016; only supporting
technical reports such as ISA-TR18.2.3 have been updated since, most
recently in 2024).
</p>

<hr>

<h2>Status</h2>

<p>Actively in development.</p>

<ul>
<li>✅ Ingestion + fault detection rules</li>
<li>✅ Column Report (filter / sort / search, stack-based navigation)</li>
<li>⬜ Flood detection / alarm clustering (linked lists)</li>
</ul>

<hr>

<h2>Credits</h2>

<table>
<tr>
<td align="center"><b>Ashwin Nair</b><br><a href="https://github.com/ashwinprakashnair-cmyk">github.com/ashwinprakashnair-cmyk</a></td>
<td align="center"><b>Rohit Kedari</b><br><a href="https://github.com/Rohitkedari-git">github.com/Rohitkedari-git</a></td>
</tr>
</table>

<p>This project uses real SCADA data published by:</p>
<blockquote>
Bassi, W., Rodrigues, A. L., &amp; Sauer, I. L. (2023). <i>Operation
SCADA Data of an Urban Small Wind Turbine in São Paulo, Brazil</i>
[Data set]. Zenodo. DOI:
<a href="https://doi.org/10.5281/zenodo.7348454">10.5281/zenodo.7348454</a>.
Institute of Energy and Environment (IEE), University of São Paulo
(USP), Brazil. Licensed under
<a href="https://creativecommons.org/licenses/by/4.0/">Creative Commons Attribution 4.0 International (CC-BY-4.0)</a>.
</blockquote>

<p>
We gratefully acknowledge the original authors for making this
real-world dataset publicly available for research and educational
reference.
</p>
