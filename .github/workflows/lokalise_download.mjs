import { execFile } from 'node:child_process';
import { setTimeout } from 'node:timers/promises';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

const DEFAULT_MAX_RETRIES = 5;
const DEFAULT_SLEEP = 1;    // in seconds
const MAX_SLEEP = 60;       // exponential backoff limit
const MAX_TOTAL = 300;      // total time limit in seconds
const DEFAULT_TIMEOUT = 120; // download command timeout (sec)

const envOr = (name, def) => process.env[name] ?? def;
const parseIntEnv = (value, def) => isNaN(parseInt(value)) ? def : parseInt(value);
const parseBoolEnv = (value) => ['true','1','yes'].includes(String(value).toLowerCase());

const config = {
    token:           envOr('LOKALISE_TOKEN', ''),
    projectId:       envOr('PROJECT_ID', ''),
    fileFormat:      envOr('FILE_FORMAT', ''),
    refName:         envOr('GITHUB_REF_NAME',''),
    skipIncludeTags: parseBoolEnv(envOr('SKIP_INCLUDE_TAGS', false)),
    maxRetries:      parseIntEnv(envOr('MAX_RETRIES', DEFAULT_MAX_RETRIES), DEFAULT_MAX_RETRIES),
    sleepTime:       parseIntEnv(envOr('SLEEP_TIME', DEFAULT_SLEEP), DEFAULT_SLEEP),
    downloadTimeout: parseIntEnv(envOr('DOWNLOAD_TIMEOUT', DEFAULT_TIMEOUT), DEFAULT_TIMEOUT),
    otherParams:     envOr('CLI_ADD_PARAMS', '').trim(),
};

if (!config.token) {
    console.error("Error: Missing LOKALISE_TOKEN env var.");
    process.exit(1);
}
if (!config.projectId) {
    console.error("Error: Missing PROJECT_ID env var.");
    process.exit(1);
}
if (!config.fileFormat) {
    console.error("Error: Missing FILE_FORMAT env var.");
    process.exit(1);
}

const isRateLimit = (txt) => txt.includes("API request error 429");
const isNoKeysError = (txt) => txt.includes("API request error 406");

async function installLokaliseCLI(timeoutSec) {
    console.log("Installing Lokalise CLI using the official installer script...");
    const installerUrl = "https://raw.githubusercontent.com/lokalise/lokalise-cli-2-go/master/install.sh";

    await execFileAsync('sh', ['-c', `curl -sfL ${installerUrl} | sh`], { timeout: timeoutSec * 1000 });

    console.log("Lokalise CLI installed successfully.");
}

async function executeDownload(cmdPath, args, timeoutSec) {
    const { stdout, stderr } = await execFileAsync(cmdPath, args, { timeout: timeoutSec * 1000 });
    const output = stdout + stderr;
    if (isNoKeysError(output))   throw new Error(output);
    if (isRateLimit(output))     throw new Error(output);
    return output;
}

async function downloadFiles(cfg) {
    console.log("Starting download from Lokalise...");

    const args = [
        `--token=${cfg.token}`,
        `--project-id=${cfg.projectId}`,
        'file', 'download',
        `--format=${cfg.fileFormat}`,
        '--original-filenames=true',
        `--directory-prefix=/`,
    ];

    if (!config.skipIncludeTags && config.refName) {
        args.push(`--include-tags=${config.refName}`);
    }

    let attempt = 1, sleepTime = cfg.sleepTime;
    const startTime = Date.now();

    while (attempt <= cfg.maxRetries) {
        console.log(`Attempt ${attempt} of ${cfg.maxRetries}...`);

        try {
            await executeDownload('./bin/lokalise2', args, cfg.downloadTimeout);
            return;
        } catch (err) {
            const msg = err.message || String(err);
            if (isNoKeysError(msg)) {
                throw new Error("No keys found for export with current settings.");
            }
            if (isRateLimit(msg)) {
                const elapsed = (Date.now() - startTime) / 1000;
                if (elapsed >= MAX_TOTAL) {
                    throw new Error(`Max total time exceeded after ${attempt} attempts.`);
                }
                console.warn(`Rate-limited. Retrying in ${sleepTime}s...`);
                await setTimeout(sleepTime * 1000);
                sleepTime = Math.min(sleepTime * 2, MAX_SLEEP);
            } else {
                console.error("Unexpected error:", msg);
            }
        }
        attempt++;
    }
    throw new Error(`Failed to download files after ${cfg.maxRetries} attempts.`);
}

try {
    await installLokaliseCLI(DEFAULT_TIMEOUT);
    await downloadFiles(config);
    console.log("Successfully downloaded files.");
    process.exit(0);
} catch (err) {
    console.error("Error:", err.message || err);
    process.exit(1);
}