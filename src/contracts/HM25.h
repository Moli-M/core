using namespace QPI;

struct HM252
{
};

struct HM25 : public ContractBase
{
public:
    // --- Funciones básicas (ERC20 y estadísticas) ---
    struct Echo_input {};
    struct Echo_output {};

    struct Burn_input {};
    struct Burn_output {};

    struct SetToken_input
    {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };
    struct SetToken_output {};

    struct GetToken_input {};
    struct GetToken_output
    {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };

    struct GetStats_input {};
    struct GetStats_output
    {
        uint64 numberOfEchoCalls;
        uint64 numberOfBurnCalls;
    };

    // ERC20: BalanceOf
    struct BalanceOf_input
    {
        id account;
    };
    struct BalanceOf_output
    {
        uint64 balance;
    };

    // ERC20: Transfer
    struct Transfer_input
    {
        id to;
        uint64 amount;
    };
    struct Transfer_output {};

    // --- Funciones DAO ---
    struct CreateProposal_input
    {
        char description[64];
    };
    struct CreateProposal_output {};

    struct VoteProposal_input
    {
        uint64 proposalId;
        bool voteFor;
    };
    struct VoteProposal_output {};

    struct ExecuteProposal_input
    {
        uint64 proposalId;
    };
    struct ExecuteProposal_output {};

private:
    // Variables de estadísticas
    uint64 numberOfEchoCalls;
    uint64 numberOfBurnCalls;

    // Datos del token (nombre, símbolo y totalSupply)
    struct Token {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };
    Token token;

    // Simulamos un mapping (address => balance) usando un array
    static const uint64 MAX_HOLDERS = 1024;
    struct BalanceEntry {
        id holder;
        uint64 balance;
    };
    Array<BalanceEntry, MAX_HOLDERS> balances;
    uint64 numHolders;

    // Estructuras para la DAO
    struct VoteRecord {
        id voter;
        uint64 weight;
        bool voteFor;
    };

    static const uint64 MAX_PROPOSALS = 128;
    static const uint64 MAX_VOTES_PER_PROPOSAL = MAX_HOLDERS;
    struct Proposal {
        uint64 proposalId;
        char description[64];
        uint64 votesFor;
        uint64 votesAgainst;
        bool executed;
        Array<VoteRecord, MAX_VOTES_PER_PROPOSAL> voteRecords;
        uint64 numVotes;
    };
    Array<Proposal, MAX_PROPOSALS> proposals;
    uint64 numProposals;

    // --- Funciones auxiliares que usan state ---
    struct FindBalanceIndex_input {
        id account;
    };
    struct FindBalanceIndex_output {
        int index;
    };
    
    PRIVATE_PROCEDURE(FindBalanceIndex)
        const id& account = input.account;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == account) {
                output.index = i;
                return;
            }
        }
        output.index = -1;
    _

    struct AddBalanceEntry_input {
        id account;
        uint64 amount;
    };
    struct AddBalanceEntry_output {};
    
    PRIVATE_PROCEDURE(AddBalanceEntry)
        const id& account = input.account;
        uint64 amount = input.amount;
        if (state.numHolders < MAX_HOLDERS) {
            BalanceEntry entry;
            entry.holder = account;
            entry.balance = amount;
            state.balances.set(state.numHolders, entry);
            state.numHolders++;
        }
        // Si se excede MAX_HOLDERS, se debería gestionar el error.
    _

    // --- Funciones DAO internas ---

    // Crea una nueva propuesta
    PUBLIC_PROCEDURE(CreateProposal)
        if (state.numProposals >= MAX_PROPOSALS) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal newProp;
        newProp.proposalId = state.numProposals;
        for (int i = 0; i < 64; i++) {
            newProp.description[i] = input.description[i];
        }
        newProp.votesFor = 0;
        newProp.votesAgainst = 0;
        newProp.executed = false;
        newProp.numVotes = 0;
        state.proposals.set(state.numProposals, newProp);
        state.numProposals++;
    _

    // Vota sobre una propuesta
    PUBLIC_PROCEDURE(VoteProposal)
        if (input.proposalId >= state.numProposals) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal prop = state.proposals.get(input.proposalId);
        if (prop.executed) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        for (int i = 0; i < (int)prop.numVotes; i++) {
            if (prop.voteRecords.get(i).voter == qpi.invocator()) {
                qpi.transfer(qpi.invocator(), qpi.invocationReward());
                return;
            }
        }
        FindBalanceIndex_input findInput;
        findInput.account = qpi.invocator();
        FindBalanceIndex_output findOutput;
        FindBalanceIndex(findInput, findOutput);
        int idx = findOutput.index;
        uint64 weight = (idx >= 0) ? state.balances.get(idx).balance : 0;
        if (weight == 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        VoteRecord vote;
        vote.voter = qpi.invocator();
        vote.weight = weight;
        vote.voteFor = input.voteFor;
        if (prop.numVotes < MAX_VOTES_PER_PROPOSAL) {
            prop.voteRecords.set(prop.numVotes, vote);
            prop.numVotes++;
        }
        if (input.voteFor) {
            prop.votesFor += weight;
        } else {
            prop.votesAgainst += weight;
        }
        state.proposals.set(input.proposalId, prop);
    _

    // Ejecuta una propuesta
    PUBLIC_PROCEDURE(ExecuteProposal)
        if (input.proposalId >= state.numProposals) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal prop = state.proposals.get(input.proposalId);
        if (prop.executed) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        if (prop.votesFor > prop.votesAgainst) {
            prop.executed = true;
        }
        state.proposals.set(input.proposalId, prop);
    _

    // --- Funciones ERC20 y estadísticas ---

    PUBLIC_PROCEDURE(Echo)
        state.numberOfEchoCalls++;
        if (qpi.invocationReward() > 0)
        {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
    _

    PUBLIC_PROCEDURE(Burn)
        state.numberOfBurnCalls++;
        if (qpi.invocationReward() > 0)
        {
            qpi.burn(qpi.invocationReward());
        }
    _

    // Establece el token y asigna el totalSupply al creador
    PUBLIC_PROCEDURE(SetToken)
        Token tempToken;
        for (int i = 0; i < 20; i++) {
            tempToken.name[i] = input.name[i];
        }
        for (int i = 0; i < 10; i++) {
            tempToken.symbol[i] = input.symbol[i];
        FindBalanceIndex_input findInput;
        findInput.account = qpi.invocator();
        FindBalanceIndex_output findOutput;
        FindBalanceIndex(findInput, findOutput);
        int index = findOutput.index;
        tempToken.totalSupply = input.totalSupply;
            AddBalanceEntry_input addInput;
            addInput.account = qpi.invocator();
            addInput.amount = input.totalSupply;
            AddBalanceEntry_output addOutput;
            AddBalanceEntry(addInput, addOutput);

        int index = findBalanceIndex(qpi.invocator());
        if (index >= 0) {
            BalanceEntry entry = state.balances.get(index);
            entry.balance = input.totalSupply;
            state.balances.set(index, entry);
        } else {
            addBalanceEntry(qpi.invocator(), input.totalSupply);
        }
    _

    PUBLIC_FUNCTION(GetToken)
        for (int i = 0; i < 20; i++) {
            output.name[i] = state.token.name[i];
        }
        for (int i = 0; i < 10; i++) {
        FindBalanceIndex_input findInput;
        findInput.account = input.account;
        FindBalanceIndex_output findOutput;
        FindBalanceIndex(findInput, findOutput);
        int index = findOutput.index;
        }
        output.totalSupply = state.token.totalSupply;
    _

    PUBLIC_FUNCTION(GetStats)
        output.numberOfEchoCalls = state.numberOfEchoCalls;
        output.numberOfBurnCalls = state.numberOfBurnCalls;
        FindBalanceIndex_input findInput;
        findInput.account = sender;
        FindBalanceIndex_output findOutput;
        FindBalanceIndex(findInput, findOutput);
        int senderIndex = findOutput.index;

    // Consulta el balance de una cuenta
    PUBLIC_FUNCTION(BalanceOf)
        int index = findBalanceIndex(input.account);
        if (index >= 0) {
            BalanceEntry entry = state.balances.get(index);
            output.balance = entry.balance;
        } else {
        FindBalanceIndex_input recipientFindInput;
        recipientFindInput.account = input.to;
        FindBalanceIndex_output recipientFindOutput;
        FindBalanceIndex(recipientFindInput, recipientFindOutput);
        int recipientIndex = recipientFindOutput.index;
        }
            AddBalanceEntry_input addInput;
            addInput.account = input.to;
            addInput.amount = input.amount;
            AddBalanceEntry_output addOutput;
            AddBalanceEntry(addInput, addOutput);

    // Realiza una transferencia de tokens
    PUBLIC_PROCEDURE(Transfer)
        id sender = qpi.invocator();
        int senderIndex = findBalanceIndex(sender);
        if (senderIndex < 0) {
            qpi.transfer(sender, qpi.invocationReward());
            return;
        }
        BalanceEntry senderEntry = state.balances.get(senderIndex);
        if (senderEntry.balance < input.amount) {
            qpi.transfer(sender, qpi.invocationReward());
            return;
        }
        senderEntry.balance -= input.amount;
        state.balances.set(senderIndex, senderEntry);

        int recipientIndex = findBalanceIndex(input.to);
        if (recipientIndex >= 0) {
            BalanceEntry recipientEntry = state.balances.get(recipientIndex);
            recipientEntry.balance += input.amount;
            state.balances.set(recipientIndex, recipientEntry);
        } else {
            addBalanceEntry(input.to, input.amount);
        }
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        // ERC20 y estadísticas
        REGISTER_USER_PROCEDURE(Echo, 1);
        REGISTER_USER_PROCEDURE(Burn, 2);
        REGISTER_USER_PROCEDURE(SetToken, 3);
        REGISTER_USER_PROCEDURE(Transfer, 4);

        REGISTER_USER_FUNCTION(GetStats, 1);
        REGISTER_USER_FUNCTION(GetToken, 2);
        REGISTER_USER_FUNCTION(BalanceOf, 3);

        // Funciones DAO
        REGISTER_USER_PROCEDURE(CreateProposal, 5);
        REGISTER_USER_PROCEDURE(VoteProposal, 6);
        REGISTER_USER_PROCEDURE(ExecuteProposal, 7);
    _

    INITIALIZE
        state.numberOfEchoCalls = 0;
        state.numberOfBurnCalls = 0;
        state.token.totalSupply = 25;
        for (int i = 0; i < 20; i++) {
            state.token.name[i] = 0;
        }
        for (int i = 0; i < 10; i++) {
            state.token.symbol[i] = 0;
        }
        state.numHolders = 0;
        state.numProposals = 0;
    _
};