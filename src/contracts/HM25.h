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

    // --- Helper functions que usan state ---
    // Declaramos findBalanceIndex como función privada:
    struct FindBalanceIndex_input {
        id account;
    };
    struct FindBalanceIndex_output {
        int index;
    };
    PRIVATE_FUNCTION_WITH_LOCALS(findBalanceIndex)
        output.index = -1;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == input.account) {
                output.index = i;
                break;
            }
        }
    _

    // Declaramos addBalanceEntry como procedimiento privado:
    struct AddBalanceEntry_input {
        id account;
        uint64 amount;
    };
    struct AddBalanceEntry_output {
        // sin salida
    };
    PRIVATE_PROCEDURE_WITH_LOCALS(addBalanceEntry)
        if (state.numHolders < MAX_HOLDERS) {
            BalanceEntry entry;
            entry.holder = input.account;
            entry.balance = input.amount;
            state.balances.set(state.numHolders, entry);
            state.numHolders++;
        }
    _

    // --- Funciones DAO internas ---
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
        {
            // Usamos la función findBalanceIndex para obtener el peso (balance) del votante.
            FindBalanceIndex_input fbInput;
            fbInput.account = qpi.invocator();
            FindBalanceIndex_output fbOutput;
            findBalanceIndex(qpi, state, fbInput, fbOutput, locals);
            uint64 weight = (fbOutput.index >= 0) ? state.balances.get(fbOutput.index).balance : 0;
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
        }
    _

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

    PUBLIC_PROCEDURE(SetToken)
        Token tempToken;
        for (int i = 0; i < 20; i++) {
            tempToken.name[i] = input.name[i];
        }
        for (int i = 0; i < 10; i++) {
            tempToken.symbol[i] = input.symbol[i];
        }
        tempToken.totalSupply = input.totalSupply;
        token = tempToken;

        {
            // Usamos findBalanceIndex para ver si ya existe saldo para el creador.
            FindBalanceIndex_input fbInput;
            fbInput.account = qpi.invocator();
            FindBalanceIndex_output fbOutput;
            findBalanceIndex(qpi, state, fbInput, fbOutput, locals);
            if (fbOutput.index >= 0) {
                BalanceEntry entry = state.balances.get(fbOutput.index);
                entry.balance = input.totalSupply;
                state.balances.set(fbOutput.index, entry);
            } else {
                AddBalanceEntry_input abInput;
                abInput.account = qpi.invocator();
                abInput.amount = input.totalSupply;
                AddBalanceEntry_output abOutput;
                addBalanceEntry(qpi, state, abInput, abOutput, locals);
            }
        }
    _

    PUBLIC_FUNCTION(GetToken)
        for (int i = 0; i < 20; i++) {
            output.name[i] = token.name[i];
        }
        for (int i = 0; i < 10; i++) {
            output.symbol[i] = token.symbol[i];
        }
        output.totalSupply = token.totalSupply;
    _

    PUBLIC_FUNCTION(GetStats)
        output.numberOfEchoCalls = state.numberOfEchoCalls;
        output.numberOfBurnCalls = state.numberOfBurnCalls;
    _

    PUBLIC_FUNCTION(BalanceOf)
        {
            FindBalanceIndex_input fbInput;
            fbInput.account = input.account;
            FindBalanceIndex_output fbOutput;
            findBalanceIndex(qpi, state, fbInput, fbOutput, locals);
            if (fbOutput.index >= 0) {
                BalanceEntry entry = state.balances.get(fbOutput.index);
                output.balance = entry.balance;
            } else {
                output.balance = 0;
            }
        }
    _

    PUBLIC_PROCEDURE(Transfer)
        {
            id sender = qpi.invocator();
            FindBalanceIndex_input fbInput;
            fbInput.account = sender;
            FindBalanceIndex_output fbOutput;
            findBalanceIndex(qpi, state, fbInput, fbOutput, locals);
            if (fbOutput.index < 0) {
                qpi.transfer(sender, qpi.invocationReward());
                return;
            }
            BalanceEntry senderEntry = state.balances.get(fbOutput.index);
            if (senderEntry.balance < input.amount) {
                qpi.transfer(sender, qpi.invocationReward());
                return;
            }
            senderEntry.balance -= input.amount;
            state.balances.set(fbOutput.index, senderEntry);

            fbInput.account = input.to;
            findBalanceIndex(qpi, state, fbInput, fbOutput, locals);
            if (fbOutput.index >= 0) {
                BalanceEntry recipientEntry = state.balances.get(fbOutput.index);
                recipientEntry.balance += input.amount;
                state.balances.set(fbOutput.index, recipientEntry);
            } else {
                AddBalanceEntry_input abInput;
                abInput.account = input.to;
                abInput.amount = input.amount;
                AddBalanceEntry_output abOutput;
                addBalanceEntry(qpi, state, abInput, abOutput, locals);
            }
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
        token.totalSupply = 25;
        for (int i = 0; i < 20; i++) {
            token.name[i] = 0;
        }
        for (int i = 0; i < 10; i++) {
            token.symbol[i] = 0;
        }
        state.numHolders = 0;
        state.numProposals = 0;
    _
};
