const { TextEncoder, TextDecoder } = require("util");
const { Api, JsonRpc } = require("eosjs");
const { JsSignatureProvider } = require("eosjs/dist/eosjs-jssig");
const nodeFetch = require("node-fetch");

const config = {
    endpoint: "https://jungle2.cryptolions.io:443",
    dgoods: {
        account: "chaingoods31",
        privateKey: ""
    }
}

const rpc = new JsonRpc(config.endpoint, { fetch: nodeFetch });

const api = new Api({
    rpc,
    signatureProvider: new JsSignatureProvider([config.dgoods.privateKey]),
    textDecoder: new TextDecoder(),
    textEncoder: new TextEncoder()
});

async function performDgoodsAccountMigrations() {
    console.log("Starting Dgoods account migration");

    const accounts = await getUniqueAccounts();
    console.log(`Found ${accounts.length} unique Dgoods owners`);

    await migrateAccounts(accounts);
    console.log("Completed");
}

async function migrateAccounts(accounts) {
    for (const account of accounts) {
        try {
            await migrateAccount(account);
            console.log(`${account} migrated`);
        } catch (e) {
            // Account has already been migrated, skipping.
            if (e.json.error.what.includes("could not insert object")) {
                console.log(`${account} already migrated, skipping`);
            } else {
                throw e;
            }
        }
    }
}

async function getUniqueAccounts() {
    const avatars = await getDgoods();
    const owners = getOwners(avatars);

    return [...new Set(owners)];
}

async function getDgoods() {
    let lastId = -1;
    let more = true;
    const avatars = [];

    while(more) {
        const result = await rpc.get_table_rows({
            json: true,
            code: config.dgoods.account,
            scope: config.dgoods.account,
            table: "dgood",
            lower_bound: lastId + 1,
            upper_bound: undefined,
            limit: 50,
        });

        avatars.push(...result.rows);
        
        more = result.more;
        lastId = result.rows.length > 0 ? result.rows[result.rows.length - 1].id : lastId;
    }

    return avatars;
}

function getOwners(dgoods) {
    return dgoods.map(dgood => dgood.owner);
}

function migrateAccount(account) {
    return api.transact(
        {
            actions: [{
                account: config.dgoods.account,
                name: "migrateaccs",
                authorization: [{ actor: config.dgoods.account, permission: "active" }],
                data: { owner: account, quantity: 5 }
            }]
        },
        { expireSeconds: 30, blocksBehind: 3 }
    );
}

performDgoodsAccountMigrations();