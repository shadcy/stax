import re
with open('drivers/jbfont.c', 'r') as f:
    content = f.read()
data_str = re.search(r'static const uint8_t Unnamed_data\[\d+\] = \{(.*?)\};', content, re.DOTALL)
if data_str:
    data = [int(x.strip(), 16) for x in data_str.group(1).split(',') if x.strip()]
    print(f"Loaded {len(data)} bytes")
    print(f"Max value: {max(data)}")
    print(f"Non-zero count: {sum(1 for x in data if x > 0)}")
    print(f"Greater than 127 count: {sum(1 for x in data if x > 127)}")
