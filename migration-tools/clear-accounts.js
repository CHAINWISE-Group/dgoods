const { api, config, getUniqueAccounts } = require("./shared");

async function performDgoodsOldAccountRemoval() {
    console.log("Starting Dgoods old accounts removal");

    const accounts = await getUniqueAccounts();
    console.log(`Found ${accounts.length} unique Dgoods owners`);

    await deleteAccounts(accounts);
    console.log("Completed");
}

async function deleteAccounts(accounts) {
    for (const account of accounts) {
        await deleteAccount(account);
        console.log(`old ${account} account deleted`);
    }
}

function deleteAccount(account) {
    return api.transact(
        {
            actions: [{
                account: config.dgoods.account,
                name: "clearaccs",
                authorization: [{ actor: config.dgoods.account, permission: "active" }],
                data: { owner: account, quantity: 5 }
            }]
        },
        { expireSeconds: 30, blocksBehind: 3 }
    );
}

performDgoodsOldAccountRemoval();