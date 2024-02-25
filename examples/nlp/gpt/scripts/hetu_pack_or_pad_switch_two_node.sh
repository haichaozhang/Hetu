# NCCL_DEBUG=info
NUM_LAYERS=${1:-32}
HIDDEN_SIZE=${2:-4096}
NUM_HEADS=${3:-32}
SEQ_LEN=${4:-128}
GLOBAL_BATCH_SIZE=${5:-32}
MICRO_BATCH_SIZE=${6:-2}

ROOT_FOLDER=data
JSON_FILE=${ROOT_FOLDER}/web/refinedweb0.json
JSON_KEY=content
VOCAB_FILE=${ROOT_FOLDER}/vocab.json
MERGE_FILE=${ROOT_FOLDER}/merges.txt

# PATH="/home/pkuhetu/envs/miniconda3/envs/hetu/bin:${PATH}"
HETU_HOME="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../" && pwd )"
LD_LIBRARY_PATH="${HETU_HOME}/build/lib:${LD_LIBRARY_PATH}"
PYTHONPATH="${HETU_HOME}/python_refactor:${HETU_HOME}/build/lib:${PYTHONPATH}"

export NCCL_DEBUG=VERSION
export HETU_SWITCH_ALGORITHM=MULTI_NODE_ROUND_ROBIN
export HETU_SWITCH_PROFILE=NVLINK
export HETU_INTERNAL_LOG_LEVEL=WARN
mpirun --allow-run-as-root -np 16 \
-H worker-0:8,worker-1:8 \
-x LD_LIBRARY_PATH -x PYTHONPATH -x NCCL_DEBUG -x HETU_SWITCH_ALGORITHM -x HETU_SWITCH_PROFILE -x HETU_INTERNAL_LOG_LEVEL -x NCCL_IB_GID_INDEX=3 \
--output-filename logs/ds_parallel --merge-stderr-to-stdout \
python3 hetu_pack_or_pad_switch.py \
--num_strategy=5 \
--ds_parallel_config ds_parallel_config/two_node/dp2_tp4_pp2.json,ds_parallel_config/two_node/dp16.json,ds_parallel_config/two_node/dp4_tp4.json,ds_parallel_config/two_node/dp2_tp8.json,ds_parallel_config/two_node/tp16.json \
--global_batch_size $GLOBAL_BATCH_SIZE \
--micro_batch_size $MICRO_BATCH_SIZE \
--json_file $JSON_FILE \
--json_key $JSON_KEY \
--vocab_file $VOCAB_FILE \
--merge_file $MERGE_FILE \
--vocab_size 30592 \
--hidden_size $HIDDEN_SIZE \
--num_hidden_layers $NUM_LAYERS \
--num_attention_heads $NUM_HEADS \
--seq_length $SEQ_LEN \
--epochs 4 \
--steps 40 \
--lr 1e-4 \
--adam_weight_decay 0.01 \
--hidden_act relu \
--dropout_prob 0.1 \
--bf16 \
--use_flash_attn \
--use_two_node \
