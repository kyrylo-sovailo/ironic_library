/*
	Part of the Ironic Project. Redistributed under MIT License, which means:
		- Do whatever you want
		- Please keep this notice and include the license file to your project
		- I provide no warranty
	To get help with installation, visit README
	Created by @meta-chan, k.sovailo@gmail.com
	Reinventing bicycles since 2020
*/

#ifndef IR_NEURO_IMPLEMENTATION
#define IR_NEURO_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <random>
#include <time.h>
#include <math.h>

#ifdef _WIN32
	#include <share.h>
#endif

#include <ir_resource/ir_memresource.h>
#include <ir_resource/ir_file_resource.h>

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::_init_matrixes(float ***matrixes, float amplitude, FILE *file)
{
	//Initializing _weights (n - 1)
	*matrixes = (float**)malloc((_nlayers - 1) * sizeof(float*));
	if (*matrixes == nullptr) return ec::ec_alloc;
	memset(*matrixes, 0, (_nlayers - 1) * sizeof(float));

	std::default_random_engine generator;
	generator.seed((unsigned int)time(NULL));
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	for (unsigned int i = 0; i < (_nlayers - 1); i++)
	{
		//Matrixes are always one element wider than given because of adjustment
		//Allocating memory
		unsigned int nlines = _layers[i + 1];
		unsigned int linesize = _IR_NEURO_UPALIGN(Align, _layers[i] + 1);
		(*matrixes)[i] = (float*)malloc((nlines * linesize + Align - 1) * sizeof(float));
		if ((*matrixes)[i] == nullptr) return ec::ec_alloc;

		//Find aligned part
		float *matrix = (float*)_IR_NEURO_BLOCK_UPALIGN((*matrixes)[i]);

		//Zero trash in beginning
		memset((*matrixes)[i], 0, (char*)matrix - (char*)(*matrixes)[i]);

		//For each line
		for (unsigned int line = 0; line < nlines; line++)
		{
			//Read data
			if (file != nullptr)
			{
				if (fread(matrix + line * linesize, sizeof(float), (_layers[i] + 1), file) < (_layers[i] + 1)) return ec::ec_read_file;
			}
			else if (amplitude != 0.0f)
			{
				for (unsigned int k = 0; k < (_layers[i] + 1); k++) matrix[line * linesize + k] = distribution(generator) * amplitude;
			}
			else
			{
				for (unsigned int k = 0; k < (_layers[i] + 1); k++) matrix[line * linesize + k] = 0.0f;
			}
			//Zero to end of line
			for (unsigned int k = _layers[i] + 1; k < linesize; k++) matrix[line * linesize + k] = 0.0f;
		}

		//Zero trash in end
		memset(matrix + nlines * linesize, 0,
			(char*)((*matrixes)[i] + nlines * linesize + Align - 1) - (char*)(matrix + nlines * linesize));
	}

	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::_init(float amplitude, FILE *file)
{
	ec code = _init_matrixes(&_weights, amplitude, file);
	if (code != ec::ec_ok) return code;

	code = _init_matrixes(&_prev_changes, 0.0f, file);
	if (code != ec::ec_ok) return code;

	//Initializing _vectors (n)
	_vectors = (float**)malloc(_nlayers * sizeof(float*));
	if (_vectors == nullptr) return ec::ec_alloc;
	memset(_vectors, 0, _nlayers * sizeof(float));

	for (unsigned int i = 0; i < _nlayers; i++)
	{
		unsigned int linesize = _IR_NEURO_UPALIGN(Align, _layers[i] + 1);
		_vectors[i] = (float *)malloc((linesize + Align - 1) * sizeof(float));
		if (_vectors[i] == nullptr) return ec::ec_alloc;
		//Zero everyting
		memset(_vectors[i], 0, (linesize + Align - 1) * sizeof(float));
		//Find aligned part
		float *vector = (float*)_IR_NEURO_BLOCK_UPALIGN(_vectors[i]);
		//Adjustment = 1.0f
		vector[_layers[i]] = 1.0f;
	}

	//Initializing _errors (n - 1)
	_errors = (float**)malloc((_nlayers - 1) * sizeof(float*));
	if (_errors == nullptr) return ec::ec_alloc;
	memset(_errors, 0, (_nlayers - 1) * sizeof(float));

	for (unsigned int i = 0; i < (_nlayers - 1); i++)
	{
		unsigned int linesize = _IR_NEURO_UPALIGN(Align, _layers[i + 1]);
		_errors[i] = (float *)malloc((linesize + Align - 1) * sizeof(float));
		if (_errors[i] == nullptr) return ec::ec_alloc;
		//Zero everyting
		memset(_errors[i], 0, (linesize + Align - 1) * sizeof(float));
	}

	//Initializing _goal
	unsigned int linesize = _IR_NEURO_UPALIGN(Align, _layers[_nlayers - 1]);
	_goal = (float *)malloc((linesize + Align - 1) * sizeof(float));
	if (_goal == nullptr) return ec::ec_alloc;
	memset(_goal, 0, (linesize + Align - 1) * sizeof(float));
	
	_ok = true;
	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::_init_from_random(unsigned int nlayers, const unsigned int *layers, float amplitude)
{
	_nlayers = nlayers;
	if (nlayers < 2) return ec::ec_invalid_input;
	for (unsigned int i = 0; i < nlayers; i++)
	{
		if (layers[i] == 0) return ec::ec_invalid_input;
	}
	_layers = (unsigned int*)malloc(nlayers * sizeof(unsigned int));
	if (_layers == nullptr) return ec::ec_alloc;
	memcpy(_layers, layers, nlayers * sizeof(unsigned int));
	return _init(amplitude, nullptr);
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::_init_from_file(const syschar *filepath)
{
	#ifdef _WIN32
		FileResource file = _wfsopen(filepath, L"rb", _SH_DENYNO);
	#else
		FileResource file = fopen(filepath, "rb");
	#endif

	if (file == nullptr) return ec::ec_open_file;

	FileHeader header, sample;
	if (fread(&header, sizeof(FileHeader), 1, file) == 0	||
		memcmp(header.signature, sample.signature, 3) != 0	||
		header.version != sample.version) return ec::ec_invalid_signature;
	
	unsigned int nlayers;
	if (fread(&nlayers, sizeof(unsigned int), 1, file) == 0) return ec::ec_read_file;
	
	MemResource<unsigned int> layers = (unsigned int*)malloc(nlayers * sizeof(unsigned int));
	if (fread(layers, sizeof(unsigned int), nlayers, file) < nlayers) return ec::ec_read_file;
	
	return _init(nlayers, layers, 0.0f, file);
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::_save_matrixes(float*const*const matrixes, FILE *file) const
{
	for (unsigned int i = 0; i < (_nlayers - 1); i++)
	{
		unsigned int nlines = _layers[i + 1];
		unsigned int linesize = _IR_NEURO_UPALIGN(Align, _layers[i] + 1);
		float *matrix = (float*)_IR_NEURO_BLOCK_UPALIGN(matrixes[i]);
		for (unsigned int line = 0; line < nlines; line++)
		{
			if (fwrite(matrix + line * linesize, sizeof(float), (_layers[i] + 1), file) < (_layers[i] + 1)) return ec::ec_read_file;
		}
	}

	return ec::ec_ok;
}

template <class ActivationFunction, unsigned int Align>
ir::Neuro<ActivationFunction, Align>::Neuro(unsigned int nlayers, const unsigned int *layers, float amplitude, ec *code)
{
	ec c = _init_from_random(nlayers, layers, amplitude);
	if (code != nullptr) *code = c;
};

template <class ActivationFunction, unsigned int Align>
ir::Neuro<ActivationFunction, Align>::Neuro(const syschar *filepath, ec *code)
{
	ec c = _init_from_file(filepath);
	if (code != nullptr) *code = c;
};

template <class ActivationFunction, unsigned int Align>
void ir::Neuro<ActivationFunction, Align>::_free_vectors(float **vector, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
	{
		if (vector[i] == nullptr) return;
		else free(vector[i]);
	}
	free(vector);
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::set_input(const float *input, bool copy)
{
	if (!_ok) return ec::ec_object_not_inited;
	if (input == nullptr) return ec::ec_null;
	if (copy)
	{
		memcpy(_IR_NEURO_BLOCK_UPALIGN(_vectors[0]), input, _layers[0] * sizeof(float));
		_user_input = nullptr;
	}
	else
	{
		if ((const float*)_IR_NEURO_BLOCK_UPALIGN(input) != input) return ir::ec::ec_align;	//check alignment
		if (input[_layers[0]] != 1.0f) return ir::ec::ec_align;								//check adjustment
		for (unsigned int i = _layers[0] + 1; i < _IR_NEURO_UPALIGN(Align, _layers[0] + 1); i++)
		{
			if (input[i] != 0.0f) return ir::ec::ec_align;									//check zeros in end
		}
		_user_input = input;
	}
	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::set_goal(const float *goal, bool copy)
{
	if (!_ok) return ec::ec_object_not_inited;
	if (goal == nullptr) return ec::ec_null;
	if (copy)
	{
		memcpy(_IR_NEURO_BLOCK_UPALIGN(_goal), goal, _layers[_nlayers - 1] * sizeof(float));
		_user_goal = nullptr;
	}
	else
	{
		if ((const float*)_IR_NEURO_BLOCK_UPALIGN(goal) != goal) return ir::ec::ec_align;
		for (unsigned int i = _layers[_nlayers - 1] + 1; i < _IR_NEURO_UPALIGN(Align, _layers[_nlayers - 1]); i++)
		{
			if (goal[i] != 0.0f) return ir::ec::ec_align;
		}
		_user_goal = goal;
	}
	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::set_coefficient(float coefficient)
{
	if (!_ok) return ec::ec_object_not_inited;
	if (coefficient < 0.0f) return ec::ec_invalid_input;
	_coefficient = coefficient;
	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::set_inductance(float inductance)
{
	if (!_ok) return ec::ec_object_not_inited;
	if (inductance < 0.0f) return ec::ec_invalid_input;
	_inductance = inductance;
	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::set_output_pointer(float *output, bool copy)
{
	if (!_ok) return ec::ec_object_not_inited;
	if (output == nullptr) return ec::ec_null;
	if (!copy)
	{
		if ((float*)_IR_NEURO_BLOCK_UPALIGN(output) != output) return ir::ec::ec_align;
		for (unsigned int i = _layers[_nlayers - 1] + 1; i < _IR_NEURO_UPALIGN(Align, _layers[_nlayers - 1]); i++)
		{
			if (output[i] != 0.0f) return ir::ec::ec_align;
		}		
	}
	_user_output = output;
	_copy_output = copy;

	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::get_output() const
{
	if (!_ok) return ec::ec_object_not_inited;
	if (_user_output != nullptr && _copy_output)
		memcpy(_user_output, _IR_NEURO_BLOCK_UPALIGN(_vectors[_nlayers - 1]), _layers[_nlayers - 1] * sizeof(float));
	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::forward()
{
	if (!_ok) return ec::ec_object_not_inited;
	
	for (unsigned int i = 0; i < (_nlayers - 1); i++)
	{
		//Calculating output for latout [i + 1] from [i]
		_step_forward(_IR_NEURO_BLOCK_UPALIGN(_weights[i]),
			_layers[i],
			(i == 0 && _user_input != nullptr) ? (FloatBlock*)_user_input : _IR_NEURO_BLOCK_UPALIGN(_vectors[i]),
			_layers[i + 1],
			(i == (_nlayers - 2) && _user_output != nullptr && !_copy_output) ? _user_output : (float*)_IR_NEURO_BLOCK_UPALIGN(_vectors[i + 1]));
	}

	return ec::ec_ok;
}

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::backward()
{
	if (!_ok) return ec::ec_object_not_inited;
	
	//Calculating error for layer [_nlayer - 1] from target and output of layer [_nlayer - 1]
	_last_backward(_layers[_nlayers - 1],
		_user_goal != nullptr ? (FloatBlock*)_user_goal : _IR_NEURO_BLOCK_UPALIGN(_goal),
		(_user_output != nullptr && !_copy_output) ? (FloatBlock*)_user_output : _IR_NEURO_BLOCK_UPALIGN(_vectors[_nlayers - 1]),
		_IR_NEURO_BLOCK_UPALIGN(_errors[_nlayers - 2]));

	for (unsigned int i = _nlayers - 2; i > 0; i--)
	{
		//Calculating error for layer [i] from output of [i] and error of [i + 1]
		_step_backward((float*)_IR_NEURO_BLOCK_UPALIGN(_weights[i]),
			_layers[i + 1], _IR_NEURO_BLOCK_UPALIGN(_errors[i]),
			_layers[i], (float*)_IR_NEURO_BLOCK_UPALIGN(_vectors[i]), (float*)_IR_NEURO_BLOCK_UPALIGN(_errors[i - 1]));
	}

	for (unsigned int i = 0; i < (_nlayers - 1); i++)
	{
		//Corrigating weights for matrix between [i] and [i + 1] from output of layer [i] and error of [i + 1]
		_corrigate(_coefficient, _inductance,
			_layers[i],
			(i == 0 && _user_input != nullptr) ? (FloatBlock*)_user_input : _IR_NEURO_BLOCK_UPALIGN(_vectors[i]),
			_layers[i + 1],
			(float*)_IR_NEURO_BLOCK_UPALIGN(_errors[i]),
			_IR_NEURO_BLOCK_UPALIGN(_weights[i]), _IR_NEURO_BLOCK_UPALIGN(_prev_changes[i]));
	}

	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::ec ir::Neuro<ActivationFunction, Align>::save(const syschar *filepath) const
{
	if (!_ok) return ec::ec_object_not_inited;

	#ifdef _WIN32
		FileResource file = _wfsopen(filepath, L"wb", _SH_DENYNO);
	#else
		FileResource file = fopen(filepath, "wb");
	#endif

	if (file == nullptr) return ec::ec_create_file;

	FileHeader header;
	if (fwrite(&header, sizeof(FileHeader), 1, file) == 0) return ec::ec_write_file;

	if (fwrite(&_nlayers, sizeof(unsigned int), 1, file) == 0) return ec::ec_write_file;
	if (fwrite(_layers, sizeof(unsigned int), _nlayers, file) < _nlayers) return ec::ec_write_file;

	ec code = _save_matrixes(_weights, file);
	if (code != ec::ec_ok) return code;
	
	code = _save_matrixes(_prev_changes, file);
	if (code != ec::ec_ok) return code;

	return ec::ec_ok;
};

template <class ActivationFunction, unsigned int Align>
ir::Neuro<ActivationFunction, Align>::~Neuro()
{
	if (_vectors != nullptr) _free_vectors(_vectors, _nlayers);
	if (_errors != nullptr) _free_vectors(_errors, _nlayers - 1);
	if (_weights != nullptr) _free_vectors(_weights, _nlayers - 1);
	if (_prev_changes != nullptr) _free_vectors(_prev_changes, _nlayers - 1);
	if (_goal != nullptr) free(_goal);
	if (_layers != nullptr) free(_layers);
};

#endif	//#ifndef IR_NEURO_IMPLEMENTATION