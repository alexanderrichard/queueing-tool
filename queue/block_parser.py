#!/usr/bin/python2.7

import re

class Block(object):

    def __init__(self):
        self.values = dict()
        self.values['name'] = 'unkown-job'
        self.values['threads'] = 1
        self.values['memory'] = 1024
        self.values['gpus'] = 0
        self.values['hours'] = 1
        self.values['subtasks'] = 1
        self.values['script'] = []

    def check(self):
        try:
            self.values['name'] = str(self.values['name'])
            self.values['threads'] = max(1, int(self.values['threads']))
            self.values['memory'] = max(1, int(self.values['memory']))
            self.values['gpus'] = max(0, int(self.values['gpus']))
            self.values['hours'] = max(1, int(self.values['hours']))
            self.values['subtasks'] = max(1, int(self.values['subtasks']))
        except:
            print 'Parser: Invalid block definition. Abort.'
            exit()

    def exists(self, key):
        return key in self.values.keys()

    def set_value(self, key, value):
        self.values[str(key)] = value

    def get_value(self, key):
        assert (self.exists(str(key)))
        return self.values[str(key)]


class Block_Parser(object):

    def __init__(self):
        self.blocks = []

    def _parse_block(self, block):
        # convert gpu=True/gpu=False to gpus=1/gpus=0 for compatibility with old scripts
        block[0] = re.sub('gpu=true', 'gpus=1', block[0])
        block[0] = re.sub('gpu=false', 'gpus=0', block[0])
        # extract job parameters
        meta = re.sub('\).*', '', re.sub('.*\(', '', block[0])).split(',')
        parsed_block = Block()
        for param in meta:
            if len(param.split('=')) == 2:
                key = param.split('=')[0].replace(' ', '')
                value = param.split('=')[1].replace(' ', '')
                if parsed_block.exists(key):
                    parsed_block.set_value(key, value)
        # create job scripts
        parsed_block.set_value('script', block[1:])
        # check block
        parsed_block.check()
        return parsed_block

    def parse(self, script):
        try:
            f = open(script, 'r')
            content = f.read().split('\n')
            f.close()
        except:
            print 'Parser: script %s could not be opened.' % script
            exit()
        # extract blocks from script
        block_starts = []
        for i, line in enumerate(content):
            if line[0:6] == '#block':
                block_starts.append(i)
        block_starts.append(len(content)) # indicate end of file
        blocks = []
        for i in range(len(block_starts) - 1):
            blocks.append( content[block_starts[i]:block_starts[i+1]] )
        # at least one block has to exist
        if len(blocks) == 0:
            print 'Error: No block defined in %s' % script
            exit()
        # parse each single block
        parsed_blocks = []
        for block in blocks:
            parsed_blocks.append( self._parse_block(block) )
        return parsed_blocks

#p = Block_Parser()
#p.parse('script.sh')
