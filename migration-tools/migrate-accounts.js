const { api, config, getUniqueAccounts } = require("./shared");

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