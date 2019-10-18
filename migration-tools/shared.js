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

const getUniqueAccounts = async () => {
  const avatars = await getDgoods();
  const owners = getOwners(avatars);

  return [...new Set(owners)];
}

const getDgoods = async () => {
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

const getOwners = (dgoods) => {
  return dgoods.map(dgood => dgood.owner);
}


module.exports = {
  config,
  rpc,
  api,
  getUniqueAccounts,
  getDgoods,
  getOwners
};